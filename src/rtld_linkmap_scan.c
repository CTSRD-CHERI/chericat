/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Capabilities Limited
 *
 * This software was developed by Capabilities Limited under Innovate UK
 * project 10027440, "Developing and Evaluating an Open-Source Desktop for Arm
 * Morello".
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

#define __CHERI_PURE_CAPABILITY__
#define RTLD_SANDBOX

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <sqlite3.h>
#include <time.h>
#include <unistd.h>
#include <rtld.h>

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <libprocstat.h>
#include <sys/link_elf.h>

#include <libxo/xo.h>
#include <sys/ptrace.h>

#include "common.h"
#include "ptrace_utils.h"
#include "rtld_linkmap_scan.h"

void getprocs_with_procstat_sysctl(sqlite3 *db, char* arg_pid) 
{
	struct procstat *psp;
	struct kinfo_proc *kipp;
	psp = procstat_open_sysctl();
	assert(psp != NULL);

	int pid = atoi(arg_pid);
        unsigned int pcnt;

	kipp = procstat_getprocs(psp, KERN_PROC_PID, pid, &pcnt);
	if (kipp == NULL) {
		err(1, "Unable to attach to process with pid %d, does it exist?", pid);
	}
	if (pcnt != 1) {
		err(1, "procstat did not get expected result from process %d, instead of a process count of 1, it got %d", pid, pcnt);
	}

	scan_rtld_linkmap(pid, psp, kipp);

	if (kipp != NULL) {
		procstat_freeprocs(psp, kipp);
	}
	procstat_close(psp);
}

/*              
 * scan_rtld_linkmap
 * When the -l option is used to attach this tool to a running process, 
 * this is called to scan the ELF aux vector to obtain the location of 
 * the dynamic table, which can then be traced to find the entry to the
 * rtld link_map.
 */
struct compart_data_list* scan_rtld_linkmap(int pid, struct procstat *psp, struct kinfo_proc *kipp)
{
	// ***** AUXV --> PHDR ***** //
	// First we need to use the auxiliary vector obtained by procstat to find out
	// the location, size and number of the PHDR
	Elf_Auxinfo *auxv;
	unsigned int auxvcnt;
	const Elf_Phdr *phdr;
	int phent, phnum;
	auxv = procstat_getauxv(psp, kipp, &auxvcnt);
	if (auxv == NULL) {
		err(1, "Unable to obtain the auxiliary vector from target process %d, does chericat have the right privilege?", pid);
	}

	for (int i=0; i<auxvcnt; i++) {
		switch (auxv[i].a_type) {
		case AT_PHDR:
			phdr = (const Elf_Phdr *)auxv[i].a_un.a_ptr;
			break;
		case AT_PHENT:
			phent = auxv[i].a_un.a_val;
			break;
		case AT_PHNUM:
			phnum = auxv[i].a_un.a_val;
			break;
		default:
			break;
		}
	}
	assert(phent == sizeof(Elf_Phdr));
        debug_print(DEBUG, "phdr: %p phent: %d phnum: %d\n", phdr, phent, phnum);

	// ***** PHDR --> PT_DYNAMIC ***** //
	// Using the PHDR address, read the program header entries of the target
	// process space into memory using ptrace
	ptrace_attach(pid);
	
	Elf_Phdr *target_phdr = calloc(phent, phnum);
	struct ptrace_io_desc piod;

	piod.piod_op = PIOD_READ_D;
	piod.piod_addr = target_phdr;
	piod.piod_offs = (void*)phdr;
	piod.piod_len = phent*phnum;

	int retno = ptrace(PT_IO, pid, (caddr_t)&piod, 0);
	
	if (retno == -1) {
		err(1, "ptrace(PT_IO) failed to scan process %d at %p", pid, phdr);
	}
	if (piod.piod_len != phent*phnum) {
		err(1, "ptrace(PT_IO) short read: %zu vs %d", piod.piod_len, phent*phnum);
	}
	debug_print(DEBUG, "target_phdr: %p piod_offs: %p piod_len: %zu\n", target_phdr, piod.piod_offs, piod.piod_len);
	
	// Scan the program header entries from read memory to find the PT_DYNAMIC section
	void *dyn = malloc(sizeof(void*));
	int dyn_size=0;
	
	for (int i=0; i<phnum; i++) {
		if (target_phdr[i].p_type == PT_DYNAMIC) {
			dyn = (void *)target_phdr[i].p_vaddr;
			dyn_size = target_phdr[i].p_memsz;
			debug_print(DEBUG, "Found it: %p\n", dyn);
			break;
		}
	}
	assert(dyn != NULL);

	// ***** PT_DYNAMIC --> DT_DEBUG ***** //
	// Now that we have the address of the dynamic section, we can use ptrace 
	// to scan the memory to obtain the data stored in it.
	Elf_Dyn *target_dyn = calloc(dyn_size, sizeof(*target_dyn));
	struct ptrace_io_desc dyn_piod;

	int phoff = sizeof(Elf_Ehdr);
	uint64_t elf_base = (char*)phdr - phoff;
	
	dyn_piod.piod_op = PIOD_READ_D;
	dyn_piod.piod_addr = target_dyn;
	dyn_piod.piod_offs = dyn + elf_base;
	dyn_piod.piod_len = dyn_size;

	int dyn_retno = ptrace(PT_IO, pid, (caddr_t)&dyn_piod, 0);
	
	if (dyn_retno == -1) {
		err(1, "ptrace(PT_IO) failed to scan process %d at %p", pid, dyn_piod.piod_offs);
	}
	if (dyn_piod.piod_len != dyn_size) {
		err(1, "ptrace(PT_IO) short read: %zu vs %d", dyn_piod.piod_len, dyn_size);
	}
	debug_print(DEBUG, "target_dyn: %p piod_offs: %p dyn_size: %d piod_len: %zu\n", target_dyn, dyn_piod.piod_offs, dyn_size, dyn_piod.piod_len);
	
	void *debug = malloc(sizeof(void*));

	for (int i=0; i<dyn_size; i++) {
		if (target_dyn[i].d_tag == DT_DEBUG) {
			debug = target_dyn[i].d_un.d_ptr;
			debug_print(DEBUG, "Found it!! %lu\n", target_dyn[i].d_un.d_ptr);
			break;
		}
	}

	assert(debug != NULL);

	// ***** DT_DEBUG --> Linkmap ***** //
	// We now have the address of the debug section on the dynamic table
	// next step is then to use the same ptrace trick to get the data at this 
	// address in the target process. This data contains the rtld link_map 
	// we are looking for.
	struct r_debug target_debug;
	struct ptrace_io_desc debug_piod;

	debug_piod.piod_op = PIOD_READ_D;
	debug_piod.piod_addr = &target_debug;
	debug_piod.piod_offs = debug;
	debug_piod.piod_len = sizeof(struct r_debug);

	int debug_retno = ptrace(PT_IO, pid, (caddr_t)&debug_piod, 0);
	
	if (debug_retno == -1) {
		err(1, "ptrace(PT_IO) failed to scan process %d at %p", pid, debug_piod.piod_offs);
	}
	if (debug_piod.piod_len != sizeof(struct r_debug)) {
		err(1, "ptrace(PT_IO) short read: %zu vs %lu", debug_piod.piod_len, sizeof(struct r_debug));
	}
	debug_print(DEBUG, "target_debug: %p piod_offs: %p piod_len: %zu\n", target_debug, debug_piod.piod_offs, debug_piod.piod_len);

	struct link_map *r_map = target_debug.r_map;

	Obj_Entry target_obj_entry; 
	struct ptrace_io_desc r_map_piod;
	r_map_piod.piod_op = PIOD_READ_D;
	r_map_piod.piod_addr = &target_obj_entry;
	r_map_piod.piod_offs = __containerof(r_map, Obj_Entry, linkmap);
	r_map_piod.piod_len = sizeof(Obj_Entry);

	int r_map_retno = ptrace(PT_IO, pid, (caddr_t)&r_map_piod, 0);

	if (r_map_retno == -1) {
		err(1, "ptrace(PT_IO) failed to scan process %d at %p", pid, debug_piod.piod_offs);
	}

	debug_print(DEBUG, "target_obj_entry: %p piod_offs: %p piod_len: %zu\n", target_obj_entry, r_map_piod.piod_offs, r_map_piod.piod_len);

	// target_link_map is a linked list, we need to traverse the entries by following l_next and look up the 
	// Obj_Entry data in the target process for each l_addr
	// We can now fill in the compart data structure to return.
	
	Obj_Entry entry = target_obj_entry;
	struct compart_data_list *comparts_head = NULL;

	while (entry.linkmap.l_next != NULL) {

		struct ptrace_io_desc name_piod;
		char *path = calloc(sizeof(char), 100);
		name_piod.piod_op = PIOD_READ_D;
		name_piod.piod_addr = path;
		name_piod.piod_offs = entry.linkmap.l_name;
		name_piod.piod_len = sizeof(char)*100;

		int retno = ptrace(PT_IO, pid, (caddr_t)&name_piod, 0);
	
		int compart_id = entry.compart_id;
		
		// Ready to move to the next link_map
		Obj_Entry *next_entry = entry.linkmap.l_next;
		struct ptrace_io_desc next_piod;
		next_piod.piod_op = PIOD_READ_D;
		next_piod.piod_addr = &entry;
		next_piod.piod_offs = __containerof(next_entry, Obj_Entry, linkmap);
		next_piod.piod_len = sizeof(Obj_Entry);

		retno = ptrace(PT_IO, pid, (caddr_t)&next_piod, 0);
		
		debug_print(DEBUG, "entry: %p path: %s compart_id:%d\n", entry, path, compart_id);
	
		compart_data_t data;
		data.path =path;
		data.id = compart_id;

		struct compart_data_list *comparts;
		comparts = (struct compart_data_list*)malloc(sizeof(struct compart_data_list));
		comparts->data = data;
		comparts->next = comparts_head;
		comparts_head = comparts;
		
		free(next_entry);
	}

	ptrace_detach(pid);
	free(target_phdr);
	free(target_dyn);
	free(dyn);
	free(debug);
	free(r_map);

	if (auxv != NULL) {
		procstat_freeauxv(psp, auxv);
	}
	return comparts_head;
}

