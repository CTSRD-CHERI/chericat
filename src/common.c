/*-
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

#include <err.h>
#include "common.h"
#include "stdio.h"
#include "stdarg.h"
#include <sys/ptrace.h>

int print_level;

void set_print_level(int level) {
	print_level = level;
}

void format_string(char *fmt, va_list argptr, char *formatted_string);

void debug_print(int level, const char *fmt, ...) 
{
	if (level <= print_level) {
		va_list argptr;
		va_start(argptr, fmt);
		vfprintf(stdout, fmt, argptr);
		va_end(argptr);
	}
}


void piod_read(int pid, int op, void *remote, void *local, size_t len)
{
    struct ptrace_io_desc piod;
    piod.piod_op = op;
    piod.piod_offs = remote; 
    piod.piod_addr = local;
    piod.piod_len = len;

    printf("remote: %#p local: %#p\n", remote, local);

    int retno = ptrace(PT_IO, pid, (caddr_t)&piod, 0);
    if (retno == -1) {
	errx(1, "ptrace(PT_IO) failed to scan process %d at remote address %p", pid, remote);
    }
    if (piod.piod_len != len) {
	errx(1, "ptrace(PT_IO) short read: %zu vs %zu", piod.piod_len, len);
    }
}
		
