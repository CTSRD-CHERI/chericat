#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sqlite3.h>

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

bool string_seen_before(char *string_array[], char *string)
{
	//int size = sizeof(string_array)/sizeof(char*);
	int size = 50;
	for (int i=0; i<size; i++) {
		if (strcmp(string_array[i], string) == 0) {
			return 1;
		}
	}
	return 0;
}


/*
 * scan_mem
 * When the -s option is used to attach this tool to a running process.
 * Uses ptrace to trace the mapped memory and persis the data to a db
 */
void scan_mem(sqlite3 *db, char* arg_pid) 
{
        int pid = atoi(arg_pid);
        char buffer[256];

	debug_print(TROUBLESHOOT, "Key Stage: Create database and tables to store all the captured data\n", NULL);
	create_vm_cap_db(db);

	debug_print(TROUBLESHOOT, "Key Stage: Attach process %d using ptrace\n", pid);
        ptrace_attach(pid);

	//ptrace_vm_struct is used to store the values obtained from pTrace request PT_VM_ENTRY
        struct ptrace_vm_entry ptrace_vm_struct;
        ptrace_vm_struct.pve_entry = 0;
        ptrace_vm_struct.pve_pathlen=sizeof(buffer);
        ptrace_vm_struct.pve_path = buffer;

	debug_print(TROUBLESHOOT, "Key Stage: Start pTrace for request PT_VM_ENTRY\n", NULL);

	char *insert_vm_query_values;

	int count = 0;

	/* Maintain the list of paths that have already been had their ELF parsed, so that
	 * we don't duplicate data or scan unnecessarily.
	 * Choosing 100 as the max number for now, ideally it should be able to grow with the 
	 * number of paths identified.
	 */
	char **seen_paths = calloc(50*sizeof(char*), sizeof(char*));
	int seen_index = 0;

	while (ptrace(PT_VM_ENTRY, pid, (caddr_t)&ptrace_vm_struct, 0) == 0) {
		// vm_cap_info has a different structure from ptrace_vm_struct that it contains 
		// cap_info as pTrace captures it.
		Vm_capture_struct vm_cap_info = {};
                
		vm_cap_info.path = malloc(strlen(ptrace_vm_struct.pve_path) + 1);
		assert(vm_cap_info.path != NULL);

                if (ptrace_vm_struct.pve_pathlen == 0) {
			strcpy(vm_cap_info.path, "unknown");
		} else {
			// ptrace_vm_struct.pve_path is already \0 terminating?
			memcpy(vm_cap_info.path, ptrace_vm_struct.pve_path, strlen(ptrace_vm_struct.pve_path)+1);
		}

		vm_cap_info.start_addr = ptrace_vm_struct.pve_start;
		vm_cap_info.end_addr = ptrace_vm_struct.pve_end;

		debug_print(INFO, "%016lx %016lx %s %d\n", 
                        vm_cap_info.start_addr,
                        vm_cap_info.end_addr,
                        vm_cap_info.path,
			ptrace_vm_struct.pve_pathlen);

		char *query_value;
		asprintf(&query_value, "(\"0x%lx\", \"0x%lx\", \"%s\")", 
				vm_cap_info.start_addr,
				vm_cap_info.end_addr,
				vm_cap_info.path);
	
		if (count == 0) {
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

		if (strcmp(vm_cap_info.path, "unknown") != 0) {
			bool seen=0;

			for (int i=0; i<seen_index; i++) {
				if (strcmp(seen_paths[i], vm_cap_info.path) == 0) {
					seen = 1;
					break;
				}
			}

			if (!seen) {
				seen_paths[seen_index] = strdup(vm_cap_info.path);
				get_elf_info(
					db,
					read_elf(vm_cap_info.path), 
					vm_cap_info.path,
					vm_cap_info.start_addr);
				seen_index++;
			}
		}

		free(query_value);
		// We don't know in advance how many capabilities can be found within a vm block
		// Hence using the maximum possible number of capabilities to allocate enough memory 
		// to hold all the values
		u_long max_caps = (ptrace_vm_struct.pve_end - ptrace_vm_struct.pve_start)/(sizeof(uintcap_t));
		vm_cap_info.cap_info = malloc(max_caps*sizeof(uintcap_t)); 
		assert(vm_cap_info.cap_info != NULL);
		
		// Divide the vm block into pages, and iterate each page to find the tags that reference each
		// address within the same page.
		for (u_long start=ptrace_vm_struct.pve_start; start<ptrace_vm_struct.pve_end; start+=4096) {
			//get_tags(pid, start, &vm_cap_info, vm_cap_info.path);
			get_tags(db, pid, start, vm_cap_info.path);
		}
		free(vm_cap_info.path);
		free(vm_cap_info.cap_info);

		ptrace_vm_struct.pve_pathlen=sizeof(buffer);

		count++;
       	}
	
	free(seen_paths);

	if (insert_vm_query_values != NULL) {
		char query_hdr[] = "INSERT INTO vm(start_addr, end_addr, mmap_path) VALUES";
		char *query;
		asprintf(&query, "%s%s;", query_hdr, insert_vm_query_values);
	
		int db_rc = sql_query_exec(db, query, NULL, NULL);
		debug_print(TROUBLESHOOT, "Key Stage: Inserted vm entry info to the database (rc=%d)\n", db_rc);
		free(insert_vm_query_values);
		free(query);
	}

        int err = errno;
        if (err != ENOENT) {
		fprintf(stderr, "ptrace hasn't ended gracefully: %s %d\n", strerror(err), err);
        }

        ptrace_detach(pid);
}


/*              
 * scan_mem
 * When the -s option is used to attach this tool to a running process.
 * Uses ptrace to trace the mapped memory and persis the data to a db
 */
void scan_mem_using_procstat(sqlite3 *db, char* arg_pid) 
{
	int pid = atoi(arg_pid);

	struct procstat *psp;
	struct kinfo_proc *kipp;
	struct kinfo_vmentry *freep, *kivp;
	uint pcnt, vmcnt;

	psp = procstat_open_sysctl();
	assert(psp != NULL);

	kipp = procstat_getprocs(psp, KERN_PROC_PID, pid, &pcnt);
	assert(kipp != NULL);
	assert(pcnt == 1);

	freep = procstat_getvmmap(psp, kipp, &vmcnt);
	assert(freep != NULL);

	create_vm_cap_db(db);
	debug_print(TROUBLESHOOT, "Key Stage: Attach process %d using ptrace\n", pid);

	char *insert_vm_query_values;

	/* Maintain the list of paths that have already been had their ELF parsed, so that
	 * we don't duplicate data or scan unnecessarily.
	 * Choosing 100 as the max number for now, ideally it should be able to grow with the 
	 * number of paths identified.
	 */
	char **seen_paths = calloc(50*sizeof(char*), sizeof(char*));
	int seen_index = 0;

	for (u_int i=0; i<vmcnt; i++) {
		kivp = &freep[i];

		debug_print(VERBOSE, "0x%016lx 0x%016lx %s %d %d\n", 
                        kivp->kve_start,
                        kivp->kve_end,
                        kivp->kve_path,
			kivp->kve_flags,
			kivp->kve_vn_type);

		char *query_value;
		asprintf(&query_value, "(\"0x%lx\", \"0x%lx\", \"%s\", %d, %d)", 
				kivp->kve_start,
				kivp->kve_end,
				kivp->kve_path,
				kivp->kve_flags,
				kivp->kve_vn_type);
	
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
		if (strlen(kivp->kve_path) > 0) {
			bool seen=0;

			for (int j=0; j<seen_index; j++) {
				if (strcmp(seen_paths[j], kivp->kve_path) == 0) {
					seen = 1;
					break;
				}
			}

			if (!seen) {
				seen_paths[seen_index] = strdup(kivp->kve_path);
				get_elf_info(
					db, 
					read_elf(kivp->kve_path), 
					kivp->kve_path,
					kivp->kve_start);

				seen_index++;
			}
		}
		free(query_value);
		
		// We don't know in advance how many capabilities can be found within a vm block
		// Hence using the maximum possible number of capabilities to allocate enough memory 
		// to hold all the values
		//u_long max_caps = (kivp->kve_end - kivp->kve_start)/(sizeof(uintcap_t));

		//Cap_info_struct *cap_info = malloc(max_caps*sizeof(uintcap_t)); 
		//assert(cap_info != NULL);
		
		// Divide the vm block into pages, and iterate each page to find the tags that reference each
		// address within the same page.
		ptrace_attach(pid);
		for (u_long start=kivp->kve_start; start<kivp->kve_end; start+=4096) {
			get_tags(db, pid, start, kivp->kve_path);
		}
		ptrace_detach(pid);
       	}
	
	free(seen_paths);

	if (insert_vm_query_values != NULL) {
		char query_hdr[] = "INSERT INTO vm(start_addr, end_addr, mmap_path, mmap_flags, vnode_type) VALUES";
		char *query;
		asprintf(&query, "%s%s;", query_hdr, insert_vm_query_values);
	
		int db_rc = sql_query_exec(db, query, NULL, NULL);
		debug_print(TROUBLESHOOT, "Key Stage: Inserted vm entry info to the database (rc=%d)\n", db_rc);
		free(insert_vm_query_values);
		free(query);
	}

        //int err = errno;
        //if (err != ENOENT) {
	//	fprintf(stderr, "ptrace hasn't ended gracefully: %s %d\n", strerror(err), err);
        //}

        //ptrace_detach(pid);

	//}

	procstat_freevmmap(psp, freep);
	procstat_freeprocs(psp, kipp);
	procstat_close(psp);
}

