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

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <sqlite3.h>
#include <time.h>
#include <libprocstat.h>

#include <libxo/xo.h>
#include <cheri/cheric.h>

#include "mem_scan.h"
#include "common.h"
#include "ptrace_utils.h"
#include "db_process.h"
#include "cap_capture.h"
#include "elf_utils.h"
#include "rtld_linkmap_scan.h"

/* _is_substring_of
 * an internal routine to check if s1 is a substring of s2
 */
int _is_substring_of(char* s1, char* s2)
{
	int s1_len = strlen(s1);
	int s2_len = strlen(s2);
	for (int i=0; i<=s2_len-s1_len; i++) {
		int j;
		for (j=0; j<s1_len; j++) {
			if (s2[i+j] != s1[j])
				break;
		}
		if (j == s1_len) {
			return i;
		}
	}
	return -1;
}

/*              
 * scan_mem
 * When the -s option is used to attach this tool to a running process.
 * Uses ptrace to trace the mapped memory and persis the data to a db
 */
void scan_mem(sqlite3 *db, int pid) 
{
	struct procstat *psp;
	struct kinfo_proc *kipp;
	struct kinfo_vmentry *freep, *kivp;
	uint pcnt, vmcnt;

	psp = procstat_open_sysctl();
	assert(psp != NULL);

	kipp = procstat_getprocs(psp, KERN_PROC_PID, pid, &pcnt);
	if (kipp == NULL) {
		errx(1, "Unable to attach to process with pid %d, does it exist?", pid);
	}
	if (pcnt != 1) {
		errx(1, "procstat did not get expected result from process %d", pid);
	}

	freep = procstat_getvmmap(psp, kipp, &vmcnt);
	if (freep == NULL) {
		errx(1, "Unable to obtain the vm map information from process %d, does chericat have the right privilege?", pid);
	}

	create_vm_cap_db(db);
	create_comparts_table(db);

	debug_print(TROUBLESHOOT, "Key Stage: Attach process %d using ptrace\n", pid);

	char *insert_vm_query_values;

	struct r_debug obtained_r_debug;
	obtained_r_debug = get_r_debug(pid, psp, kipp);
	compart_data_list *scanned_comparts = scan_rtld_linkmap(pid, db, obtained_r_debug);
	scan_r_comparts(pid, db, obtained_r_debug);

	/* Maintain the list of paths that have already been had their ELF parsed, so that
	 * we don't duplicate data or scan unnecessarily.
	 */
	size_t seen_kivp_buffer_size;
	size_t seen_kivp_buffer_capacity;
	struct kinfo_vmentry *seen_kivp;

	const int initial_size = 100;
	seen_kivp_buffer_size = 0;
	seen_kivp_buffer_capacity = initial_size;
	seen_kivp = calloc(initial_size, sizeof(struct kinfo_vmentry));
	if (!seen_kivp) {
		errx(1, "Cannot allocate %lu bytes for the kivp wrapper", initial_size*sizeof(struct kinfo_vmentry));
	}


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
				if (seen_kivp_buffer_size == seen_kivp_buffer_capacity) {
					seen_kivp_buffer_capacity *= 2;
					seen_kivp = realloc(seen_kivp, seen_kivp_buffer_capacity*sizeof(struct kinfo_vmentry));
					if (!seen_kivp) {
						errx(1, "Out of memory, cannot grow the size of the kivp array any more, current size is: %lu", seen_kivp_buffer_size);
					}
				}				
				seen_kivp[seen_index] = *kivp;
				seen_kivp_buffer_size++;
				
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
					mmap_path = strdup("Heap(others)");
				}
			}
		} else {
			mmap_path = strdup(kivp->kve_path);
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

		compart_data_list *comparts_head = scanned_comparts;

		int compart_id = -1;

		while (comparts_head != NULL) {
			compart_data current_compart_data = comparts_head->data;
			if ((current_compart_data.path != NULL) && strncmp(current_compart_data.path, mmap_path, strlen(current_compart_data.path)) == 0) {
				compart_id = current_compart_data.id;
				break;
			}
			comparts_head = comparts_head->next;
		}

		debug_print(INFO, "0x%016lx 0x%016lx %s %d %d %d %d\n", 
                        kivp->kve_start,
                        kivp->kve_end,
                        mmap_path,
			compart_id,
			kivp->kve_protection,
			kivp->kve_flags,
			kivp->kve_type);

		char *query_value;
		asprintf(&query_value, "(\"0x%lx\", \"0x%lx\", \"%s\", %d, %d, %d, %d)", 
				kivp->kve_start,
				kivp->kve_end,
				mmap_path,
				compart_id,
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
		
		// Divide the vm block into 4k pages, and iterate each page to find the tags that reference each
		// address within the same page.
		ptrace_attach(pid);
		// If the vm block does not allow cap read or write, skip the capability scan
		if (kivp->kve_flags & KVME_FLAG_HASCAP) { 
			for (u_long start=kivp->kve_start; start<kivp->kve_end; start+=4096) {
				get_tags(db, pid, start, mmap_path);
			}
		}
		ptrace_detach(pid);
	}
	
	free(seen_kivp);

	if (insert_vm_query_values != NULL) {
		char query_hdr[] = "INSERT INTO vm(start_addr, end_addr, mmap_path, compart_id, kve_protection, mmap_flags, vnode_type) VALUES";
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
			"plt_addr=\"0x%lx\", "
			"plt_size=\"0x%lx\", "
			"got_addr=\"0x%lx\", "
			"got_size=\"0x%lx\" " 
			"WHERE mmap_path=\"%s\";",
			ssect[i].plt_addr, 
			ssect[i].plt_size,
			ssect[i].got_addr, 
			ssect[i].got_size, 
			ssect[i].mmap_path);
	
		sql_query_exec(db, query, NULL, NULL);
		free(query);
	}

	free(scanned_comparts);
	procstat_freevmmap(psp, freep);
	procstat_freeprocs(psp, kipp);
	procstat_close(psp);
}

