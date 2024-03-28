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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <libxo/xo.h>

#include "db_process.h"

static void get_filename_from_path(char *path, char **filename) {
        char slash = '/';
        char *ptr = strrchr(path, slash);

        if (ptr) {
                *filename = strdup(ptr+1); //Move pointer one to the right to not include the '/' itself
        } else {
                *filename = strdup(path);
        }
}

/*
 * vm_caps_view
 * SQLite callback routine to traverse results from sqlite_exec()
 * using:
 * 
 * 1. "SELECT * FROM vm;"
 * Results are formatted into: start_addr, end_addr, mmap_path
 *
 * 2. "SELECT * FROM cap_info WHERE cap_loc_addr >= start_addr AND cap_loc_addr <= end_addr;"
 * Results are added: no_of_caps(%), no_of_ro_caps, no_of_rw_caps, no_of_x_caps
 * 
 */
void vm_caps_view(sqlite3 *db) 
{
	vm_info *vm_info_captured;
	int vm_count = get_all_vm_info(db, &vm_info_captured);
	assert(vm_count != -1);

	cap_info *cap_info_captured;
	int cap_count = get_all_cap_info(db, &cap_info_captured);
	assert(cap_count != -1);

	int dbname_len = strlen(get_dbname());
	for (int l=0; l<dbname_len+4; l++) {	
		xo_emit("{:/-}");
	}
	xo_emit("{:/\n %s \n}", get_dbname());
	for (int l=0; l<dbname_len+4; l++) {	
		xo_emit("{:/-}");
	}

	int ptrwidth = sizeof(void *);
	xo_emit("{T:/\n%*s %*s %6s %5s %5s %5s %5s %8s %8s %-5s %-2s %5s %-s}\n",
		ptrwidth, "START", ptrwidth-1, "END", "PRT", "ro", "rw", "rx", "rwx", "TOTAL", "DENSITY", "FLAGS", "TP", "COMPART", "PATH");
		
	xo_open_list("vm_cap_output");
	for (int i=1; i<vm_count; i++) {
		xo_open_instance("vm_cap_output");
		xo_emit("{:mmap_start_addr/%*s}", ptrwidth, vm_info_captured[i].start_addr);
		xo_emit("{:mmap_end_addr/%*s}", ptrwidth, vm_info_captured[i].end_addr);
			
		xo_emit("{:read/%3s}", vm_info_captured[i].kve_protection & KVME_PROT_READ ?
			"r" : "-");
		xo_emit("{:write/%s}", vm_info_captured[i].kve_protection & KVME_PROT_WRITE ?
			"w" : "-");
		xo_emit("{:exec/%s}", vm_info_captured[i].kve_protection & KVME_PROT_EXEC ?
			"x" : "-");
		xo_emit("{:read_cap/%s}", vm_info_captured[i].kve_protection & KVME_PROT_READ_CAP ? 
			"R" : "-");
		xo_emit("{:write_cap/%s} ", vm_info_captured[i].kve_protection & KVME_PROT_WRITE_CAP ? 
			"W" : "-");

		uintptr_t start_addr = (uintptr_t)strtol(vm_info_captured[i].start_addr, NULL, 0);
	       	uintptr_t end_addr = (uintptr_t)strtol(vm_info_captured[i].end_addr, NULL, 0);
		
		int out_cap_count=0;
		int ro_count=0;
		int rw_count=0;
		int rx_count=0;
		int rwx_count=0;

		for (int j=0; j<cap_count; j++) {
			uintptr_t cap_loc_addr = (uintptr_t)strtol(cap_info_captured[j].cap_loc_addr, NULL, 0);
			
			if (cap_loc_addr >= start_addr && cap_loc_addr <= end_addr) {
				out_cap_count++;
				if (strchr(cap_info_captured[j].perms, 'r') != NULL &&
					strchr(cap_info_captured[j].perms, 'w') == NULL &&
					strchr(cap_info_captured[j].perms, 'x') == NULL) {
					ro_count++;
				}
				if (strchr(cap_info_captured[j].perms, 'r') != NULL &&
					strchr(cap_info_captured[j].perms, 'w') != NULL &&
					strchr(cap_info_captured[j].perms, 'x') == NULL) {
					rw_count++;
				}
				if (strchr(cap_info_captured[j].perms, 'r') != NULL &&
					strchr(cap_info_captured[j].perms, 'x') != NULL &&
					strchr(cap_info_captured[j].perms, 'w') == NULL) {
					rx_count++;
				}
				if (strchr(cap_info_captured[j].perms, 'r') != NULL &&
					strchr(cap_info_captured[j].perms, 'x') != NULL &&
					strchr(cap_info_captured[j].perms, 'w') != NULL) {
					rwx_count++;
				}
			}
		}
		xo_emit("{:ro_count/%5d} ", ro_count);
		xo_emit("{:rw_count/%5d} ", rw_count);
		xo_emit("{:rx_count/%5d} ", rx_count);
		xo_emit("{:rwx_count/%5d} ", rwx_count);
		xo_emit("{:out_cap_count/%8d} ", out_cap_count);

		xo_emit("{:out_cap_density/%8.2f%%} ", ((float)out_cap_count/cap_count)*100);
			
		xo_emit("{:copy_on_write/%-1s}", vm_info_captured[i].mmap_flags &
		    	KVME_FLAG_COW ? "C" : "-");
		xo_emit("{:need_copy/%-1s}", vm_info_captured[i].mmap_flags &
		   	KVME_FLAG_NEEDS_COPY ? "N" : "-");
		xo_emit("{:super_pages/%-1s}", vm_info_captured[i].mmap_flags &
			KVME_FLAG_SUPER ? "S" : "-");
		xo_emit("{:grows_down/%-1s}", vm_info_captured[i].mmap_flags &
			KVME_FLAG_GROWS_UP ? "U" : vm_info_captured[i].mmap_flags &
	    		KVME_FLAG_GROWS_DOWN ? "D" : "-");
		xo_emit("{:wired/%-1s} ", vm_info_captured[i].mmap_flags &
	    		KVME_FLAG_USER_WIRED ? "W" : "-");
			
		const char *str;	
		switch (vm_info_captured[i].vnode_type) {
		case KVME_TYPE_NONE:
			str = "--";
			break;
		case KVME_TYPE_DEFAULT:
			str = "df";
			break;
		case KVME_TYPE_VNODE:
			str = "vn";
			break;
		case KVME_TYPE_SWAP:
			str = "sw";
			break;
		case KVME_TYPE_DEVICE:
			str = "dv";
			break;
		case KVME_TYPE_PHYS:
			str = "ph";
			break;
		case KVME_TYPE_DEAD:
			str = "dd";
			break;
		case KVME_TYPE_SG:
			str = "sg";
			break;
		case KVME_TYPE_MGTDEVICE:
			str = "md";
			break;
		case KVME_TYPE_GUARD:
			str = "gd";
			break;
		case KVME_TYPE_UNKNOWN:
		default:
			str = "??";
			break;
		}
		xo_emit("{:kve_type/%-2s} ", str);	
			
		xo_emit("{:compart_id/%7d} ", vm_info_captured[i].compart_id);

		char *filename = (char*)malloc(sizeof(vm_info_captured[i].mmap_path));
		get_filename_from_path(vm_info_captured[i].mmap_path, &filename);			
		xo_emit("{:mmap_path/%s}\n", filename);
		free(filename);

		free(vm_info_captured[i].start_addr);
		free(vm_info_captured[i].end_addr);
		free(vm_info_captured[i].mmap_path);
		
		xo_close_instance("vm_cap_output");
	}
	for (int k=0; k<cap_count; k++) {
		free(cap_info_captured[k].cap_loc_addr);
		free(cap_info_captured[k].cap_addr);
		free(cap_info_captured[k].perms);
		free(cap_info_captured[k].base);
		free(cap_info_captured[k].top);
	}
	
	xo_close_list("vm_cap_output");
	free(vm_info_captured);
	free(cap_info_captured);

}

