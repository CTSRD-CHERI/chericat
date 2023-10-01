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
	printf("In vm_caps_view, obtained vm_count = %d\n", vm_count);
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
	xo_emit("{T:/\n%*s %*s %6s %5s %5s %5s %5s %8s %8s %-5s %-2s %-s}\n",
		ptrwidth, "START", ptrwidth-1, "END", "PRT", "ro", "rw", "rx", "rwx", "TOTAL", "DENSITY", "FLAGS", "TP", "PATH");
	
	for (int i=1; i<vm_count; i++) {
		xo_open_instance("vm_cap_output");
		xo_emit("{:mmap_start_addr/%*s}", ptrwidth, vm_info_captured[i].start_addr);
		xo_emit("{:mmap_end_addr/%*s}", ptrwidth, vm_info_captured[i].end_addr);
			
		xo_emit("{d:read/%3s}", vm_info_captured[i].kve_protection & KVME_PROT_READ ?
			"r" : "-");
		xo_emit("{d:write/%s}", vm_info_captured[i].kve_protection & KVME_PROT_WRITE ?
			"w" : "-");
		xo_emit("{d:exec/%s}", vm_info_captured[i].kve_protection & KVME_PROT_EXEC ?
			"x" : "-");
		xo_emit("{d:read_cap/%s}", vm_info_captured[i].kve_protection & KVME_PROT_READ_CAP ? 
			"R" : "-");
		xo_emit("{d:write_cap/%s} ", vm_info_captured[i].kve_protection & KVME_PROT_WRITE_CAP ? 
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
			
		const char *str, *lstr;	
		switch (vm_info_captured[i].vnode_type) {
		case KVME_TYPE_NONE:
			str = "--";
			lstr = "none";
			break;
		case KVME_TYPE_DEFAULT:
			str = "df";
			lstr = "default";
			break;
		case KVME_TYPE_VNODE:
			str = "vn";
			lstr = "vnode";
			break;
		case KVME_TYPE_SWAP:
			str = "sw";
			lstr = "swap";
			break;
		case KVME_TYPE_DEVICE:
			str = "dv";
			lstr = "device";
			break;
		case KVME_TYPE_PHYS:
			str = "ph";
			lstr = "physical";
			break;
		case KVME_TYPE_DEAD:
			str = "dd";
			lstr = "dead";
			break;
		case KVME_TYPE_SG:
			str = "sg";
			lstr = "scatter/gather";
			break;
		case KVME_TYPE_MGTDEVICE:
			str = "md";
			lstr = "managed_device";
			break;
		case KVME_TYPE_GUARD:
			str = "gd";
			lstr = "guard";
			break;
		case KVME_TYPE_UNKNOWN:
		default:
			str = "??";
			lstr = "unknown";
			break;
		}
		xo_emit("{:kve_type/%-2s} ", str);	
			
		char *filename = (char*)malloc(sizeof(vm_info_captured[i].mmap_path));
		get_filename_from_path(vm_info_captured[i].mmap_path, &filename);			
		xo_emit("{:mmap_path/%s}\n", filename);
		free(filename);

		free(vm_info_captured[i].start_addr);
		free(vm_info_captured[i].end_addr);
		free(vm_info_captured[i].mmap_path);
	}
	for (int k=0; k<cap_count; k++) {
		free(cap_info_captured[k].cap_loc_addr);
		free(cap_info_captured[k].cap_addr);
		free(cap_info_captured[k].perms);
		free(cap_info_captured[k].base);
		free(cap_info_captured[k].top);
	}
	xo_close_instance("vm_cap_output");
	
	free(vm_info_captured);
	free(cap_info_captured);

}
