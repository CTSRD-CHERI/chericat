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

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/ptrace.h>

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

#include <libxo/xo.h>
#include <libprocstat.h>

#include "common.h"
#include "db_process.h"
#include "ptrace_utils.h"
#include "rtld_linkmap_scan.h"

/* START Copied from rtld-elf/rtld_c18n.c */
typedef ssize_t string_handle;

struct string_base {
    char *buf;
    string_handle size;
    string_handle capacity;
};

typedef struct compart {
    /*
     * Compartment information exposed to the kernel.
     */
    struct rtld_c18n_compart info;
    /*
     * Name of the compartment
     */
    const char *name;
    /*
     * Names of libraries that belong to the compartment
     */
    struct string_base libs;
    /*
     * Symbols that the library is allowed to import, if restrict_imports is
     * true.
     */
    struct string_base imports;
    /*
     * Symbols trusted by the library.
     */
    struct string_base trusts;
    bool restrict_imports;
} compart_t;
/* END Copied section */

void getprocs_with_procstat_sysctl(sqlite3 *db, int pid) 
{
	struct procstat *psp;
	struct kinfo_proc *kipp;
	psp = procstat_open_sysctl();
	assert(psp != NULL);

        unsigned int pcnt;

	kipp = procstat_getprocs(psp, KERN_PROC_PID, pid, &pcnt);
	if (kipp == NULL) {
		err(1, "Unable to attach to process with pid %d, does it exist?", pid);
	}
	if (pcnt != 1) {
		err(1, "procstat did not get expected result from process %d, instead of a process count of 1, it got %d", pid, pcnt);
	}

	/* These calls are currently made as part of the scan_mem routine
           because we are storing the compart_ids on the VM table as well
           as the comparts table. The reason for doing that is because 
           we cannot rely on the mapbase + mapsize from Obj_Entry structure
           to figure out the address ranges for each compartments, as some 
           of them might not have been initialised when the Obj_Entry is 
           created. Once we know how to deal with this problem we can remove 
           the calls from scan_mem and only make them here when users choose 
           the "show comp" command line option */
	//struct r_debug obtained_r_debug = get_r_debug(pid, psp, kipp);
	//scan_rtld_linkmap(pid, db, obtained_r_debug);

	if (kipp != NULL) {
		procstat_freeprocs(psp, kipp);
	}
	procstat_close(psp);
}

/*              
 * get_r_debug
 * This is called to scan the ELF aux vector to obtain the location of 
 * the dynamic table, which can then be traced to find the debug struct
 * exposed by cheribsd for debugger. This debug struct, r_debug, contains
 * the entry to the rtld link_map and compartments array.
 */
struct r_debug get_r_debug(int pid, struct procstat *psp, struct kinfo_proc *kipp)
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
    debug_print(INFO, "phdr: %p phent: %d phnum: %d\n", phdr, phent, phnum);

    // ***** PHDR --> PT_DYNAMIC ***** //
    // Using the PHDR address, read the program header entries of the target
    // process space into memory using ptrace
    ptrace_attach(pid);
	
    Elf_Phdr *target_phdr = calloc(phent, phnum);
    piod_read(pid, PIOD_READ_D, (void*)phdr, target_phdr, phent*phnum);
    debug_print(INFO, "remote_phdr: %p local_phdr: %p\n", phdr, target_phdr);

    // Scan the program header entries from read memory to find the PT_DYNAMIC section
    void *dyn = malloc(sizeof(void*));
    int dyn_size=0;
	
    for (int i=0; i<phnum; i++) {
	if (target_phdr[i].p_type == PT_DYNAMIC) {
	    dyn = (void *)target_phdr[i].p_vaddr;
	    dyn_size = target_phdr[i].p_memsz;
	    debug_print(INFO, "Found PT_DYNAMIC section: %p\n", dyn);
	    break;
	}
    }
    assert(dyn != NULL);

    // ***** PT_DYNAMIC --> DT_DEBUG ***** //
    // Now that we have the address of the dynamic section, we can use ptrace 
    // to scan the memory to obtain the data stored in it.
    Elf_Dyn *target_dyn = calloc(dyn_size, sizeof(*target_dyn));
    int phoff = sizeof(Elf_Ehdr);
    uint64_t elf_base = (uint64_t)((char*)phdr - phoff);
    piod_read(pid, PIOD_READ_D, (dyn+elf_base), target_dyn, dyn_size);
    debug_print(INFO, "remote_dyn: %p local_dyn %p\n", dyn+elf_base, target_dyn);
	
    void *remote_debug = malloc(sizeof(void*));

    for (int i=0; i<dyn_size; i++) {
	if (target_dyn[i].d_tag == DT_DEBUG) {
	    remote_debug = (void*)target_dyn[i].d_un.d_ptr;
	    debug_print(INFO, "Found r_debug: %lu\n", target_dyn[i].d_un.d_ptr);
	    break;
	}
    }
    assert(remote_debug != NULL);

    // ***** DT_DEBUG --> Linkmap ***** //
    // We now have the address of the debug section on the dynamic table
    // next step is then to use the same ptrace trick to get the data at this 
    // address in the target process. This data contains the rtld link_map 
    // we are looking for.

    struct r_debug local_debug;
    piod_read(pid, PIOD_READ_D, remote_debug, (void*)&local_debug, sizeof(struct r_debug));

    ptrace_detach(pid);
    free(target_phdr);
    free(target_dyn);

    if (auxv != NULL) {
	procstat_freeauxv(psp, auxv);
    }
    
    debug_print(INFO, "r_debug is at %p\n", local_debug);

    return local_debug;
}

/* scan_linkmap
 * Using the linkmap exposed via r_debug, we can get the list of mapped libraries and their 
 * corresponding compart_id.
 */
compart_data_list *scan_rtld_linkmap(int pid, sqlite3 *db, struct r_debug target_debug)
{
    ptrace_attach(pid);

    struct link_map *r_map = target_debug.r_map;
    compart_data_list *comparts_head = NULL;

    while (r_map != NULL) {

	Obj_Entry entry;
	void *linkmap_addr = __containerof(r_map, Obj_Entry, linkmap);
	piod_read(pid, PIOD_READ_D, linkmap_addr, &entry, sizeof(Obj_Entry));
	debug_print(INFO, "remote_next_entry: %p local_next_entry: %#p mapbase: %p mapsize: %lu\n", linkmap_addr, entry, entry.mapbase, entry.mapsize);

	char *path = get_string(pid, (psaddr_t)entry.linkmap.l_name, 0);

	char *path_name = (char*)malloc(sizeof(path));
	get_filename_from_path(path, &path_name);			

	compart_data default_data;
	default_data.id = entry.default_compart_id;
	default_data.path = path; //Need to store full path to match with mmap_path in VM table
        default_data.start_addr = (Elf_Addr)entry.mapbase;
        default_data.end_addr = (Elf_Addr)entry.mapbase + entry.mapsize;
	default_data.is_default = true;

	compart_data_list *comparts_entry;
	comparts_entry = (compart_data_list*)malloc(sizeof(compart_data_list));
	comparts_entry->data = default_data;
	comparts_entry->next = comparts_head;
	comparts_head = comparts_entry;

	char *insert_default_compart_q;
	asprintf(&insert_default_compart_q, "INSERT OR REPLACE INTO comparts(compart_id, library_path, start_addr, end_addr, is_default) VALUES (%d, \"%s\", \"0x%lx\", \"0x%lx\", %d);", default_data.id, path_name, default_data.start_addr, default_data.end_addr, default_data.is_default);

	debug_print(INFO, "Add comparts entry: %s\n", insert_default_compart_q);
	sql_query_exec(db, insert_default_compart_q, NULL, NULL);
	free(insert_default_compart_q);
	free(path_name);
	
	// If there are sub-compartments, create them too and add them to the list
	if (entry.ncomparts > 0) {
	    Compart_Entry current_subcompart;
	    void *subcompart_addr = entry.comparts;

	    for (int i=0; i<entry.ncomparts; i++) {
		// Found a sub-compartment, construct an entry and then add to the list
		piod_read(pid, PIOD_READ_D, subcompart_addr, &current_subcompart, sizeof(Compart_Entry));

		char *compart_full_name = get_string(pid, (psaddr_t)current_subcompart.name, 0);
		if (compart_full_name == NULL) {
		    // skip
		    break;
		}
		char *compart_name = (char*)malloc(sizeof(compart_full_name));
		get_filename_from_path(compart_full_name, &compart_name);			
		
		char *insert_subcomparts_q;
		asprintf(&insert_subcomparts_q, "INSERT OR REPLACE INTO comparts(compart_id, compart_name, start_addr, end_addr, is_default, parent_id) VALUES (%d, \"%s\", \"0x%lx\", \"0x%lx\", %d, %d);", current_subcompart.compart_id, compart_name, current_subcompart.start, current_subcompart.end, false, default_data.id);
		debug_print(INFO, "Add sub-comparts entry: %s\n", insert_default_compart_q);
		sql_query_exec(db, insert_subcomparts_q, NULL, NULL);
		free(insert_subcomparts_q);
		free(compart_name);

		subcompart_addr = subcompart_addr + (sizeof(Compart_Entry));
	    }
	}
        // Ready to move to the next link_map
	r_map = entry.linkmap.l_next;
    }

    ptrace_detach(pid);
    free(r_map);

    return comparts_head;
}

/* scan_r_comparts
 * Using the r_comparts array exposed via r_debug, we can obtain the list of 
 * compartments names and their ids.
 */
char **scan_r_comparts(int pid, sqlite3 *db, struct r_debug target_debug)
{
    ptrace_attach(pid);

    // In gdb, this is how the same data is extracted: 
    // ((struct compart *)r_debug->r_comparts)[r_debug->r_comparts_size]

    int comparts_size = target_debug.r_comparts_size;
    void *comparts = target_debug.r_comparts;

    // The entry size can be obtained from _compart_size which is a global extern variable,
    // it can be queried from ELF syms. During the ELF syms we can extract this value and store
    // it to a local variable or it can be queried from the elf_sym sqlite table.
    int comparts_entry_size = sizeof(compart_t);
    char **compart_names = calloc(comparts_entry_size*sizeof(char), comparts_size);

    for (int i=0; i<comparts_size; i++) {
        compart_t comparts_entry;
        piod_read(pid, PIOD_READ_D, comparts, &comparts_entry, sizeof(compart_t));
        
	comparts = comparts + sizeof(compart_t);
	
	char *compart_full_name = get_string(pid, (psaddr_t)comparts_entry.name, 0);
        if (compart_full_name != NULL) {
	    char *compart_name = (char*)malloc(sizeof(compart_full_name));
	    get_filename_from_path(compart_full_name, &compart_name);			

	    debug_print(INFO, "i: %d remote_comparts_entry: %p obtained compartment name: %s\n", i, comparts_entry, compart_name);

	    char *insert_comparts_db_table_q;
	    asprintf(&insert_comparts_db_table_q, 
		"UPDATE comparts SET "
		"compart_name=\"%s\" "
                "WHERE compart_id=%d;", compart_name, i);
	    sql_query_exec(db, insert_comparts_db_table_q, NULL, NULL);
	    free(insert_comparts_db_table_q);
	}
	compart_names[i] = compart_full_name;
    }

    ptrace_detach(pid);
    return compart_names;
}

