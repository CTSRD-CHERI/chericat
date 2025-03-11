/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Jessica Man 
 *
 * This software was developed by the University of Cambridge Computer
 * Laboratory (Department of Computer Science and Technology) as part of the
 * CHERI for Hypervisors and Operating Systems (CHaOS) project, funded by
 * EPSRC grant EP/V000292/1.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <libelf.h>
#include <gelf.h>
#include <err.h>
#include <link.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <libprocstat.h>

#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>

#include "common.h"
#include "db_process.h"
#include "ptrace_utils.h"

/*
 * ptrace_attach(int pid)
 * Calls the ptrace API to remotely attach a running process that has
 * the provided pid.
 * It waits for the attached process (the tracee) to stop via a call to 
 * waitpid(), so that it can carry out the tracing request(s).
 */
void ptrace_attach(int pid)
{
        if (ptrace(PT_ATTACH, pid, 0, 0) == -1) {
                int err = errno;
                fprintf(stderr, "ptrace attach failed: %s %d\n", strerror(err), err);
                return;
        }

        int status;

        if (waitpid(pid, &status, 0) == -1) {
                int err_waitpid = errno;
                fprintf(stderr, "ptrace waitpid failed: %s %d\n", strerror(err_waitpid), err_waitpid);
                return;
        }
}

/*
 * ptrace_detach(int pid)
 * Calls the ptrace API to detach the attached process that has the 
 * provided pid, and hence releases the tracee to resume execution in 
 * untraced mode.
 */
void ptrace_detach(int pid)
{
        if (ptrace(PT_DETACH, pid, 0, 0) == -1) {
                int err_detach = errno;
                fprintf(stderr, "ptrace detach failed: %s %d\n", strerror(err_detach), err_detach);
                return;
        }
}

void print_ptype(size_t pt) {
	char *s;
#define C(V) case PT_##V: s = #V; break 
	switch (pt) {
		C(NULL); C(INTERP); C(PHDR); 
		C(SUNWBSS); C(SUNWCAP);
		C(LOAD); C(DYNAMIC); C(NOTE); 
		C(SHLIB); C(TLS); C(SUNW_UNWIND); 
		C(SUNWSTACK); C(SUNWDTRACE);
	default:
		s = "unknown";
		break; 
	}
	printf(" \"%s\"", s); 
#undef C
}

/*
 * read_data(int pid, void *addr, void *vptr, int len)
 * Using ptrace to read data in the attached remote process's address space.
 * The ptrace request reads a single "int" of data in each call, from the provided
 * address "addr". This routine calls the request repeatedly until the full "len" 
 * has been read. The data is returned as the return value from the ptrace, 
 * and is subsequently stored using the passed-in pointer "vptr".
 */
void read_data(int pid, void *addr, void *vptr, int len)
{
	int i=0, count=0;
	long word;
	int *ptr = (int *)vptr;

	while (count < len) {
		word = ptrace(PT_READ_D, pid, ((caddr_t)addr)+count, 0);
		count += sizeof(int);
		ptr[i++] = word;
	}
}

/* 
 * read_lwps
 * Using ptrace to read info of kernel threads
 */
lwpthrs_t get_lwps_list(int pid) 
{
	int nlwps;
	lwpid_t *lwps;
	
	nlwps = ptrace(PT_GETNUMLWPS, pid, NULL, 0);
	if (nlwps == -1) {
		err(1, "Failed to get number of LWPs for process %d", pid);
	}
	assert(nlwps > 0);

	lwps = calloc(nlwps, sizeof(*lwps));
	nlwps = ptrace(PT_GETLWPLIST, pid, (caddr_t)lwps, nlwps);
	if (nlwps == -1) {
		err(1, "Failed to get LWP list for process %d", pid);
	}
	assert(nlwps > 0);
	debug_print(INFO, "Number of threads read: %d\n", nlwps);

	lwpthrs_t lwpthrs;
	lwpthrs.nlwps = nlwps;
	lwpthrs.lwps = lwps;

	return lwpthrs;
}

struct ptrace_lwpinfo read_lwpinfo(int lwps_tid)
{
	struct ptrace_lwpinfo pl;

	if (ptrace(PT_LWPINFO, lwps_tid, (caddr_t)&pl, sizeof(pl)) == -1) {
		err(1, "Failed to get LWP info");
	}
	
	debug_print(INFO, "Read thread %d: %s\n", pl.pl_lwpid, pl.pl_tdname);
	return pl;
}

void piod_read(int pid, int op, void *remote, void *local, size_t len)
{
    struct ptrace_io_desc piod;
    piod.piod_op = op;
    piod.piod_offs = remote; 
    piod.piod_addr = local;
    piod.piod_len = len;

    int retno = ptrace(PT_IO, pid, (caddr_t)&piod, 0);
    if (retno == -1) {
	errx(1, "ptrace(PT_IO) failed to scan process %d at remote address %p", pid, remote);
    }
    if (piod.piod_len != len) {
	errx(1, "ptrace(PT_IO) short read: %zu vs %zu", piod.piod_len, len);
    }
}

/*
 * Copy a string from the process.  Note that it is
 * expected to be a C string, but if max is set, it will
 * only get that much.
 */
char *
get_string(pid_t pid, psaddr_t addr, int max)
{
	struct ptrace_io_desc iorequest;
	char *buf, *nbuf;
	size_t offset, size, totalsize;

	offset = 0;
	if (max)
		size = max + 1;
	else {
		/* Read up to the end of the current page. */
		size = PAGE_SIZE - (addr % PAGE_SIZE);
		if (size > MAXSIZE)
			size = MAXSIZE;
	}
	totalsize = size;
	buf = malloc(totalsize);
	if (buf == NULL)
		return (NULL);
	for (;;) {
		iorequest.piod_op = PIOD_READ_D;
		iorequest.piod_offs = (void *)((uintptr_t)addr + offset);
		iorequest.piod_addr = buf + offset;
		iorequest.piod_len = size;
		if (ptrace(PT_IO, pid, (caddr_t)&iorequest, 0) < 0) {
			free(buf);
			return (NULL);
		}
		if (memchr(buf + offset, '\0', size) != NULL)
			return (buf);
		offset += size;
		if (totalsize < MAXSIZE && max == 0) {
			size = MAXSIZE - totalsize;
			if (size > PAGE_SIZE)
				size = PAGE_SIZE;
			nbuf = realloc(buf, totalsize + size);
			if (nbuf == NULL) {
				buf[totalsize - 1] = '\0';
				return (buf);
			}
			buf = nbuf;
			totalsize += size;
		} else {
			buf[totalsize - 1] = '\0';
			return (buf);
		}
	}
}
