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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sqlite3.h>
#include <time.h>

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <libprocstat.h>

#include <libxo/xo.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <cheri/cheric.h>

#include "common.h"
#include "ptrace_utils.h"
#include "db_process.h"
#include "cap_capture.h"
#include "elf_utils.h"

/*              
 * scan_mem
 * When the -s option is used to attach this tool to a running process.
 * Uses ptrace to trace the mapped memory and persis the data to a db
 */
void scan_mem(sqlite3 *db, char* arg_pid) 
{
	int pid = atoi(arg_pid);

	struct procstat *psp;
	struct kinfo_proc *kipp;
	struct kinfo_vmentry *freep, *kivp;
	uint pcnt, vmcnt;

	psp = procstat_open_sysctl();
	assert(psp != NULL);

	kipp = procstat_getprocs(psp, KERN_PROC_PID, pid, &pcnt);
	if (kipp == NULL) {
		fprintf(stderr, "Unable to attach to process with pid %d, does it exist?\n", pid);
		exit(1);
	}
	if (pcnt != 1) {
		fprintf(stderr, "procstat did not get expected result from process %d\n", pid);
		exit(1);
	}

	freep = procstat_getvmmap(psp, kipp, &vmcnt);
	if (freep == NULL) {
		fprintf(stderr, "Unable to obtain the vm map information from process %d, does cherciat have the right privilege?\n", pid);
		exit(1);
	}

	create_vm_cap_db(db);
	debug_print(TROUBLESHOOT, "Key Stage: Attach process %d using ptrace\n", pid);

	char *insert_vm_query_values;

	/* Maintain the list of paths that have already been had their ELF parsed, so that
	 * we don't duplicate data or scan unnecessarily.
	 * Choosing 50 as the max number for now, ideally it should be able to grow with the 
	 * number of paths identified.
	 */
	struct kinfo_vmentry *seen_kivp = calloc(50, sizeof(struct kinfo_vmentry));
	special_sections *ssect = (special_sections *)calloc(vmcnt, sizeof(special_sections));
	assert(ssect != NULL);
	int ssect_index = 0;

	int seen_index = 0;

	for (u_int i=0; i<vmcnt; i++) {
		kivp = &freep[i];

		if (strlen(kivp->kve_path) > 0) {
			bool seen=0;

			for (int j=0; j<seen_index; j++) {
				if (strcmp(seen_kivp[j].kve_path, kivp->kve_path) == 0) {
					seen = 1;
					break;
				}
			}

			if (!seen) {
				seen_kivp[seen_index] = *kivp;
				get_elf_info(
					db, 
					read_elf(kivp->kve_path), 
					kivp->kve_path,
					kivp->kve_start,
					&ssect,
					ssect_index++);
				seen_index++;
			}
		}
		char *mmap_path = NULL;

		if (strlen(kivp->kve_path) == 0) {
			int found=0;
			for (int j=0; j<seen_index; j++) {

				if (seen_kivp[j].kve_start == kivp->kve_reservation) {
					mmap_path = strdup(seen_kivp[j].kve_path);
					found = 1;
					break;
				}
			}

			if (found == 0) {

				// The mmap vm block is not within any of the loaded library range
				// now try to "guess" where it belong by using the vnode information
				if (kivp->kve_type == KVME_TYPE_GUARD) {
					mmap_path = strdup("Guard");
				} else if (kivp->kve_flags & KVME_FLAG_GROWS_DOWN) {
					mmap_path = strdup("Stack");
				} else {
					mmap_path = strdup("unknown");
				}
			}
		} else {
			mmap_path = strdup(kivp->kve_path);
		}

		// TODO: Will make these into a neater routine!
		if (kivp->kve_start <= ssect[ssect_index-1].bss_addr &&
			kivp->kve_end >= ssect[ssect_index-1].bss_addr+ssect[ssect_index-1].bss_size) {
			char *new_path;
			asprintf(&new_path, "%s(.bss)", mmap_path);
			mmap_path = strdup(new_path);
			free(new_path);
		}
		if (kivp->kve_start <= ssect[ssect_index-1].plt_addr &&
			kivp->kve_end >= ssect[ssect_index-1].plt_addr+ssect[ssect_index-1].plt_size) {
			char *new_path;
			asprintf(&new_path, "%s(.plt)", mmap_path);
			mmap_path = strdup(new_path);
			free(new_path);
		}
		if (kivp->kve_start <= ssect[ssect_index-1].got_addr &&
			kivp->kve_end >= ssect[ssect_index-1].got_addr+ssect[ssect_index-1].got_size) {
			char *new_path;
			asprintf(&new_path, "%s(.got)", mmap_path);
			mmap_path = strdup(new_path);
			free(new_path);
		}

		debug_print(INFO, "0x%016lx 0x%016lx %s %d %d %d\n", 
                        kivp->kve_start,
                        kivp->kve_end,
                        mmap_path,
			kivp->kve_protection,
			kivp->kve_flags,
			kivp->kve_type);

		char *query_value;
		asprintf(&query_value, "(\"0x%lx\", \"0x%lx\", \"%s\", %d, %d, %d)", 
				kivp->kve_start,
				kivp->kve_end,
				mmap_path,
				kivp->kve_protection,
				kivp->kve_flags,
				kivp->kve_type);
	
		if (i == 0) {
			insert_vm_query_values = (char*)malloc(sizeof(query_value));
			assert(insert_vm_query_values != NULL);

			insert_vm_query_values = strdup(query_value);
		} else {
			char *temp;
			asprintf(&temp, "%s,%s", 
				insert_vm_query_values,
				query_value);
			insert_vm_query_values = strdup(temp);
			free(temp);
		}
		free(query_value);
		
		// Divide the vm block into pages, and iterate each page to find the tags that reference each
		// address within the same page.
		ptrace_attach(pid);
		for (u_long start=kivp->kve_start; start<kivp->kve_end; start+=4096) {
			get_tags(db, pid, start, mmap_path);
		}
		ptrace_detach(pid);
       	}
	
	free(seen_kivp);

	if (insert_vm_query_values != NULL) {
		char query_hdr[] = "INSERT INTO vm(start_addr, end_addr, mmap_path, kve_protection, mmap_flags, vnode_type) VALUES";
		char *query;
		asprintf(&query, "%s%s;", query_hdr, insert_vm_query_values);
	
		int db_rc = sql_query_exec(db, query, NULL, NULL);
		debug_print(TROUBLESHOOT, "Key Stage: Inserted vm entry info to the database (rc=%d)\n", db_rc);
		free(insert_vm_query_values);
		free(query);
	}

	// Also persist the bss, plt and got info for this source to the same elf_sym table
	for (int i=0; i<ssect_index; i++) {
		char *query;
		asprintf(&query,	
			"UPDATE vm SET "
			"bss_addr=\"0x%lx\", "
			"bss_size=\"0x%lx\", "
			"plt_addr=\"0x%lx\", "
			"plt_size=\"0x%lx\", "
			"got_addr=\"0x%lx\", "
			"got_size=\"0x%lx\" " 
			"WHERE mmap_path=\"%s\";",
			ssect[i].bss_addr, 
			ssect[i].bss_size, 
			ssect[i].plt_addr, 
			ssect[i].plt_size,
			ssect[i].got_addr, 
			ssect[i].got_size, 
			ssect[i].mmap_path);
	
		sql_query_exec(db, query, NULL, NULL);
		free(query);
	}

	procstat_freevmmap(psp, freep);
	procstat_freeprocs(psp, kipp);
	procstat_close(psp);
}

