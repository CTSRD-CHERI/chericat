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

/* get_capability
 */
void get_capability(int pid, void* addr, int current_cap_count, Vm_cap_info_struct *vm_cap_info, char **query_vals)
{
	struct ptrace_io_desc piod;
	char capbuf[sizeof(uintcap_t)+1];
	
	piod.piod_op = PIOD_READ_CHERI_CAP;
        piod.piod_offs = addr;
        piod.piod_addr = capbuf;
        piod.piod_len = sizeof(capbuf);
	
        // ***** Sending IO trace request and obtain the capability pointed to by the provided address *****/
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
	vm_cap_info->cap_info[current_cap_count]->addr_at_which_cap_references = copy;
	debug_print(VERBOSE, "Address of the copied capability: %#p\n", (void*)copy);

	// Getting permissions of the obtained capability

	char str[128];
	char permsread[16];
	int tokens;
	unsigned long addrread, base, top;
	char attrread[32];

	strfcap(str, sizeof(str), "%C", copy);
	tokens = sscanf(str, "%lx [%15[^,],%lx-%lx] %31s", &addrread, permsread, &base, &top, attrread);

	debug_print(VERBOSE, "Using strfcap API to parse the cap, tokens: %d permsread: %s base: 0x%lx top: 0x%lx attrread: %31s\n", 
			tokens, permsread, base, top, attrread);

	vm_cap_info->cap_info[current_cap_count]->perms = malloc(sizeof(char)*strlen(permsread)+1);
	assert(vm_cap_info->cap_info[current_cap_count]->perms != NULL);
	
	strcpy(vm_cap_info->cap_info[current_cap_count]->perms, permsread);

	// Return the captured caps into multiple values to be inserted using a single sql statement
	int query_size = asprintf(query_vals, "(\"%p\", \"%p\", \"%s\", \"%p\", \"%p\")", 
					addr, (void*)copy, permsread, (void*)base, (void*)top);
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
void get_tags(int pid, u_long start, Vm_cap_info_struct *vm_cap_info)
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
						vm_cap_info->cap_info[cap_count] = malloc(sizeof(uintcap_t) + sizeof(uintcap_t) + sizeof(uintcap_t) + 1);
						assert(vm_cap_info->cap_info[cap_count] != NULL);

						vm_cap_info->cap_info[cap_count]->addr_of_cap = (uintptr_t)address;
						
						// Now we have enough information to go through each capability to obtain further information about them.
						char *val;
						get_capability(pid, (void*)tags_addr[j], cap_count, vm_cap_info, &val);
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
	
			int db_rc = sql_query_exec(query, NULL, NULL);
			debug_print(TROUBLESHOOT, "Key Stage: Inserted vm entry info to the database (rc=%d)\n", db_rc);
			free(query);
			free(insert_cap_query_values);
		}
	} else { 
		// This generates a lot of noise, useful for troubleshooting when needed
		debug_print(TROUBLESHOOT, "ptrace(PT_IO) for PIOD_READ_CHERI_TAGS returned %d\n", retno);
	}
	vm_cap_info->cap_count = cap_count;
}

void print_vm_block(Vm_cap_info_struct vm_cap_info, xo_handle_t *xop) {
	xo_emit_h(xop, "{:path/%s}\t{:start_addr/%08lx}\t{:end_addr/%08lx}\n",
			vm_cap_info.path,
			vm_cap_info.start_addr,
			vm_cap_info.end_addr);

	debug_print(1, "***** VM Block for %s *****\n", vm_cap_info.path);
	debug_print(1, "Start: %016lx\n", vm_cap_info.start_addr);
	debug_print(1, "End: %016lx\n", vm_cap_info.end_addr);
	debug_print(1, "No of Capabilities: %d\n", vm_cap_info.cap_count);
	for (int i=0; i<vm_cap_info.cap_count; i++) {
		debug_print(1, "     Cap[%d]: addr_of_cap: %p\n", i, (void*)vm_cap_info.cap_info[i]->addr_of_cap);
		debug_print(1, "     Cap[%d]: addr_at_which_cap_references: %p\n", i, (void*)vm_cap_info.cap_info[i]->addr_at_which_cap_references);
		debug_print(1, "     Cap[%d]: perms: %s\n", i, vm_cap_info.cap_info[i]->perms);
	}

}

