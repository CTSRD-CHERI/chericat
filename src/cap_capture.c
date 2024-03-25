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
#include <assert.h>

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <string.h>
#include <sqlite3.h>

#include <libxo/xo.h>
#include <cheri/cheric.h>

#include "common.h"
#include "db_process.h"
#include "cap_capture.h"
#include "ptrace_utils.h"

void get_capability(int pid, void* addr, int current_cap_count, char *path, char **query_vals)
{
	struct ptrace_io_desc piod;
	char capbuf[sizeof(uintcap_t)+1];
	
	piod.piod_op = PIOD_READ_CHERI_CAP;
        piod.piod_offs = addr;
        piod.piod_addr = capbuf;
        piod.piod_len = sizeof(capbuf);
	
        // Sending IO trace request and obtain the capability pointed to by the provided address
	int retno = ptrace(PT_IO, pid, (caddr_t)&piod, 0);

	if (DEBUG) {
		for (int i=0; i<sizeof(uintcap_t)+1; i++) {
        		debug_print(VERBOSE, "%02x ", capbuf[i]);
		}
		debug_print(VERBOSE, "\n", NULL);
	}

	// Getting a copy of the capability and store it to the vm_cap_info struct
	uintcap_t copy;

	memcpy(&copy, &capbuf[1], sizeof(copy));
	debug_print(VERBOSE, "Address of the copied capability: %#p\n", (void*)copy);

	// Getting permissions of the obtained capability
	char str[128];
	char permsread[16] = {};
	int tokens;
	unsigned long addrread, base, top;
	char attrread[32];

	strfcap(str, sizeof(str), "%C", copy);
	tokens = sscanf(str, "%lx [%15[^,],%lx-%lx] %31s", &addrread, permsread, &base, &top, attrread);

	debug_print(VERBOSE, "Using strfcap API to parse the cap, tokens: %d permsread: %s base: 0x%lx top: 0x%lx attrread: %31s\n", 
			tokens, permsread, base, top, attrread);

	// Return the captured caps into multiple values to be inserted using a single sql statement
	int query_size = asprintf(query_vals, "(\"%p\", \"%s\", \"%p\", \"%s\", \"%p\", \"%p\")", 
					addr, path, (void*)copy, permsread, (void*)(uintptr_t)base, (void*)(uintptr_t)top);
	assert(query_size != -1);

        if (retno != 0) {
                fprintf(stderr, "ptrace(PT_IO) for PIOD_READ_CHERI_CAP hasn't ended gracefully: %s %d\n", strerror(retno), retno); 
	}
}

/* get_tags
 * By using the ptrace PIOD_READ_CHERI_TAGS API, this function scans each provided
 * address - u_long start - and find all the marked tags. It stores the found tags'
 * addressed to the vm_cap_info struct along with the capabilities info obtained. 
 */
int get_tags(sqlite3 *db, int pid, u_long start, char *path)
{
        struct ptrace_io_desc piod;
        char tagsbuf[32] = {};
	int cap_count = 0;
        
	piod.piod_op = PIOD_READ_CHERI_TAGS;
        piod.piod_offs = (void*)(uintptr_t)start;
        piod.piod_addr = tagsbuf;
        piod.piod_len = sizeof(tagsbuf);

        int retno = ptrace(PT_IO, pid, (caddr_t)&piod, 0);

	// retno is 0 is the address has a cheri tag
	// "start" is the address pointing to a page of addresses, each address is 64bytes 
	// so there are 256 addresses on a 4k-byte page
	//
	// pTrace scans the addresses per page, and stores their corresponding tag bit in the tagsbuf array,
	// packing each 8 bits into a char, hence there are 32 chars in the tagsbuf char array.
        if (retno == 0) {
		char *insert_cap_query_values = NULL;

        	for (int i=0; i<32; i++) {
			char tags = tagsbuf[i];
			
			// Each "char tags" contains 8 tags with 1 bit each
			uintptr_t tags_addr[8];

			if ('0' != tags) { 
				for (int j=0; j<8; j++) {
					// Checking each tag bit, if it corresponds to a capability (1) or not (0)
					int bit = tags & 1;

					// The corresponding address of each tag is offset by 8x16 from the 256 addresses we have reached 
					// so far in this loop, because each tags in tagsbuf has 8 bits, each bit correspond to a 16 bytes address.
					// We need to calculate the offset from start, hence the i*8*16 to get the starting address in each 
					// inner iteration.
					u_long address = start + i*8*16 + j*16;
					tags_addr[j] = (uintptr_t)address;

					tags = tags >> 1;
					if (bit) {
						debug_print(VERBOSE, "Addresses referenced by tags[%d] (cap_count %d): %p\n", j, cap_count, (void*)tags_addr[j]);
						// Now we have enough information to go through each capability to obtain further information about them.
						char *val;
						get_capability(pid, (void*)tags_addr[j], cap_count, path, &val);
						debug_print(VERBOSE, "Obtained cap values: %s\n", val);

						if (insert_cap_query_values == NULL) {
							insert_cap_query_values = (char*)malloc(sizeof(val));
							assert(insert_cap_query_values != NULL);
							insert_cap_query_values = strdup(val);
						} else {
							char *temp;
							asprintf(&temp, "%s,%s", insert_cap_query_values, val);
							insert_cap_query_values = strdup(temp);
							
							free(temp);
						}
						free(val);
						// Maintain a count for each vm block.
						cap_count++;
					}	
				}
			}
        	}
		if (insert_cap_query_values != NULL && insert_cap_query_values[0] != '\0') {
			char query_hdr[] = "INSERT INTO cap_info VALUES";
			char *query;
			asprintf(&query, "%s %s;", query_hdr, insert_cap_query_values);
	
			int db_rc = sql_query_exec(db, query, NULL, NULL);
			debug_print(TROUBLESHOOT, "Key Stage: Inserted vm entry info to the database (rc=%d)\n", db_rc);
			free(query);
			free(insert_cap_query_values);
		}
	} else { 
		// This generates a lot of noise, useful for troubleshooting when needed
		debug_print(TROUBLESHOOT, "ptrace(PT_IO) for PIOD_READ_CHERI_TAGS returned %d\n", retno);
	}
	return cap_count;
}

