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

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxo/xo.h>

#include "db_process.h"

/*
 * comp_caps_view
 * SQLite callback routine to traverse results from sqlite_exec()
 * using:
 * 
 * 1. "SELECT * FROM comparts;"
 * Results are formatted into: compart_id, compart_name, start_addr, end_addr
 *
 * 2. "SELECT * FROM cap_info WHERE cap_loc_addr >= start_addr AND cap_loc_addr <= end_addr;"
 * Results are added: no_of_caps(%), no_of_ro_caps, no_of_rw_caps, no_of_rx_caps, no_of_rwx_caps
 * 
 */
void comp_caps_view(sqlite3 *db) 
{
	comp_info *comp_info_captured;
	int comp_count = get_all_comp_info(db, &comp_info_captured);
	assert(comp_count != -1);

	cap_info *cap_info_captured;
	int cap_count = get_all_cap_info(db, &cap_info_captured);
	assert(cap_count != -1);

	// Obj_Entry cannot initialise mapbase and mapsize for RTLD and therefore
	// cannot rely purely on the comparts table to get the address ranges for RTLD.
	// Instead, we query the database vm table specifically to get the addresses
	rtld_addrs *rtld_addrs_captured;
	int rtld_count = get_all_rtld_addrs(db, &rtld_addrs_captured);
	assert(rtld_count != -1);

	int dbname_len = strlen(get_dbname());
	for (int l=0; l<dbname_len+4; l++) {	
		xo_emit("{:/-}");
	}
	xo_emit("{:/\n %s \n}", get_dbname());
	for (int l=0; l<dbname_len+4; l++) {	
		xo_emit("{:/-}");
	}

	int ptrwidth = sizeof(void *);
	xo_emit("{T:/\n%6s %44s %9s %44s %10s %5s %5s %5s %8s %8s}\n",
		"COMP_ID", "CAPLOC_COMP_NAME", "COMP_ID", "CAPADDR_COMP_NAME", "ro", "rw", "rx", "rwx", "TOTAL", "DENSITY");
		
	xo_open_list("comp_cap_output");
	for (int i=0; i<comp_count; i++) {
	    uintptr_t src_start_addr = 0;
	    uintptr_t src_end_addr = 0;
	    if (strcmp(comp_info_captured[i].compart_name, "[RTLD]") == 0) {
		//src_start_addr would be the first address from the captured addresses
		//src_end_addr would the last address from the captured addresses
		src_start_addr = (uintptr_t)strtol(rtld_addrs_captured[0].start_addr, NULL, 0);
		src_end_addr = (uintptr_t)strtol(rtld_addrs_captured[rtld_count-1].end_addr, NULL, 0);
	    } else {
		if (comp_info_captured[i].start_addr != NULL) {
		    src_start_addr = (uintptr_t)strtol(comp_info_captured[i].start_addr, NULL, 0);
		}
		if  (comp_info_captured[i].end_addr != NULL) {
		    src_end_addr = (uintptr_t)strtol(comp_info_captured[i].end_addr, NULL, 0);
		}
	    }
	    for (int k=0; k<comp_count; k++) {
		xo_open_instance("comp_cap_output");

		uintptr_t dest_start_addr = 0;
		uintptr_t dest_end_addr = 0;
		if (strcmp(comp_info_captured[k].compart_name, "[RTLD]") == 0) {
		    //src_start_addr would be the first address from the captured addresses
		    //src_end_addr would the last address from the captured addresses
		    dest_start_addr = (uintptr_t)strtol(rtld_addrs_captured[0].start_addr, NULL, 0);
		    dest_end_addr = (uintptr_t)strtol(rtld_addrs_captured[rtld_count-1].end_addr, NULL, 0);
		} else {
		    if (comp_info_captured[k].start_addr != NULL) {
			dest_start_addr = (uintptr_t)strtol(comp_info_captured[k].start_addr, NULL, 0);
		    }
		    if  (comp_info_captured[k].end_addr != NULL) {
			dest_end_addr = (uintptr_t)strtol(comp_info_captured[k].end_addr, NULL, 0);
		    }
		}

		int out_cap_count=0;
		int ro_count=0;
		int rw_count=0;
		int rx_count=0;
		int rwx_count=0;

		for (int j=0; j<cap_count; j++) {
		    uintptr_t cap_loc_addr = (uintptr_t)strtol(cap_info_captured[j].cap_loc_addr, NULL, 0);
		    uintptr_t cap_addr = (uintptr_t)strtol(cap_info_captured[j].cap_addr, NULL, 0);
		
		    if (cap_loc_addr >= src_start_addr && 
			cap_loc_addr <= src_end_addr && 
			cap_addr >= dest_start_addr && 
			cap_addr <= dest_end_addr) {
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
	   
		if (out_cap_count != 0) {
		    xo_emit("{:src_compart_id/%6d}", comp_info_captured[i].compart_id);
		    xo_emit("{:src_compart_name/%45s}", comp_info_captured[i].compart_name);
		
		    xo_emit("{:dest_compart_id/%10d}", comp_info_captured[k].compart_id);
		    xo_emit("{:dest_compart_name/%45s}", comp_info_captured[k].compart_name);

		    xo_emit("{:ro_count/%11d} ", ro_count);
		    xo_emit("{:rw_count/%5d} ", rw_count);
		    xo_emit("{:rx_count/%5d} ", rx_count);
		    xo_emit("{:rwx_count/%5d} ", rwx_count);
		    xo_emit("{:out_cap_count/%8d} ", out_cap_count);

		    xo_emit("{:out_cap_density/%8.2f%%}\n", ((float)out_cap_count/cap_count)*100);
		}
	    }	
	    free(comp_info_captured[i].start_addr);
	    free(comp_info_captured[i].end_addr);
	    free(comp_info_captured[i].compart_name);		
	    xo_close_instance("comp_cap_output");
	}
	for (int k=0; k<cap_count; k++) {
		free(cap_info_captured[k].cap_loc_addr);
		free(cap_info_captured[k].cap_addr);
		free(cap_info_captured[k].perms);
		free(cap_info_captured[k].base);
		free(cap_info_captured[k].top);
	}
	for (int r=0; r<rtld_count; r++) {
	    free(rtld_addrs_captured[r].start_addr);
	    free(rtld_addrs_captured[r].end_addr);
	}
	
	xo_close_list("comp_cap_output");
	free(comp_info_captured);
	free(cap_info_captured);
	free(rtld_addrs_captured);
}

