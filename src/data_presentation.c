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

typedef struct vm_info_struct {
	char *start_addr;
	char *end_addr;
	char *mmap_path;
	int mmap_flags;
	int vnode_type;
} vm_info;

typedef struct cap_info_struct {
	char *cap_loc_addr;
	char *cap_loc_path;
	char *cap_addr;
	char *perms;
	char *base;
	char *top;
} cap_info;

typedef struct sym_info_struct {
	char *source_path;
	char *sym_name;
	char *sym_offset;
	char *sym_type;
	char *shndx;
	char *type;
	char *bind;
	char *addr;
} sym_info;

vm_info *all_vm_info;
cap_info *all_cap_info;

int all_vm_info_index, all_cap_info_index;
int vm_count, cap_count;

/*
 * vm_query_callback
 * SQLite callback routine to traverse results from sqlite_exec()
 * using: "SELECT * FROM vm;"
 * Results are formatted into: start_addr, end_addr, mmap_path
 *
 * argc - number of columns
 * argv - pointer to the data that is stored in each column
 * azColName - pointer to the colunmn names
 */
static int vm_query_callback(void *NotUsed, int argc, char **argv, char **azColName)
{
	/* Database schema for vm has 3 columns */
	assert(argc == 5);

	vm_info vm_info;
	vm_info.start_addr = strdup(argv[0]);
	vm_info.end_addr = strdup(argv[1]);
	vm_info.mmap_path = strdup(argv[2]);
	vm_info.mmap_flags = atoi(argv[3]);
	vm_info.vnode_type = atoi(argv[4]);

	all_vm_info[all_vm_info_index++] = vm_info;
	
    	return 0;
}

/*
 * cap_info_query_callback
 * SQLite callback routine to traverse results from sqlite_exec()
 * using: "SELECT * FROM cap_info;"
 * Results are formatted into: cap_loc_addr, cap_addr, perms, base, top
 *
 * argc - number of columns
 * argv - pointer to the data that is stored in each column
 * azColName - pointer to the colunmn names
 */
static int cap_info_query_callback(void *NotUsed, int argc, char **argv, char **azColName)
{
	assert(argc == 6);
 		
	cap_info cap_info;
	cap_info.cap_loc_addr = strdup(argv[0]);
	cap_info.cap_loc_path = strdup(argv[1]);
	cap_info.cap_addr = strdup(argv[2]);
	cap_info.perms = strdup(argv[3]);
	cap_info.base = strdup(argv[4]);
	cap_info.top = strdup(argv[5]);

	all_cap_info[all_cap_info_index++] = cap_info;
    	
	return 0;
}

/*
 * sym_name_query_callback
 * SQLite callback routine to traverse results from sqlite_exec()
 * using: "SELECT st_name FROM elf_sym where addr == cap;"
 * Results are formatted into: sym_name
 *
 * sym_name - to store the pointer result to the symbol name string from the query
 * argc - number of columns
 * argv - pointer to the data that is stored in each column
 * azColName - pointer to the colunmn names
 */

static int sym_name_query_callback(void *sym_name, int argc, char **argv, char **azColName)
{
	char **result_ptr = (char **)sym_name;
	if (argc >= 0) {
		*result_ptr = (char*)calloc(strlen(argv[0])+1, sizeof(char));
		strcpy(*result_ptr, argv[0]);
	}
	return 0;
}

static int get_vm_count_query_callback(void *NotUsed, int argc, char **argv, char **azColName)
{
    for(int i=0; i<argc; i++){
	vm_count = atoi(argv[i]);
    }
    return 0;
}

static int get_cap_info_count_query_callback(void *NotUsed, int argc, char **argv, char **azColName)
{
    for(int i=0; i<argc; i++){
	cap_count = atoi(argv[i]);
    }
    return 0;
}

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
void vm_caps_view(sqlite3 *db) {
	/* Obtain how many vm items from the database first, we can then use it to 
	 * determine the size of the struct array for holding all the vm entries */
	int rc = sql_query_exec(db, "SELECT COUNT(*) FROM vm;", get_vm_count_query_callback, NULL); 
	assert(rc == 0);

	/* Initialise all_vm_info struct to hold the values obtained from SQL query */
	all_vm_info = calloc(vm_count+1, sizeof(vm_info));
	assert (all_vm_info != NULL);
	
	all_vm_info_index = 0;
		
	rc = rc & sql_query_exec(db, "SELECT * FROM vm;", vm_query_callback, NULL);

	/* Obtain how many cap_info items from the database first, we can then use it to 
	 * determine the size of the struct array for holding all the vm entries */
	rc = rc & sql_query_exec(db, "SELECT COUNT(*) FROM cap_info;", get_cap_info_count_query_callback, NULL); 
	assert(rc == 0);

	/* Initialise all_cap_info struct to hold the values obtained from the SQL query */
	all_cap_info = calloc(cap_count+1, sizeof(cap_info));
	assert(all_cap_info != NULL);
	
	all_cap_info_index = 0;

	rc &= sql_query_exec(db, "SELECT * FROM cap_info;", cap_info_query_callback, NULL);

	if (rc == 0) {
		int dbname_len = strlen(get_dbname());
		for (int l=0; l<dbname_len+4; l++) {	
			xo_emit("{:/-}");
		}
		xo_emit("{:/\n %s \n}", get_dbname());
		for (int l=0; l<dbname_len+4; l++) {	
			xo_emit("{:/-}");
		}

		int ptrwidth = sizeof(void *);
		xo_emit("{T:/\n%*s %*s %4s %5s %5s %8s %8s %-5s %-2s %-s}\n",
			ptrwidth, "START", ptrwidth-1, "END", "ro", "rw", "rx", "TOTAL", "DENSITY", "FLAGS", "TP", "PATH");
	
		for (int i=1; i<all_vm_info_index; i++) {
			xo_open_instance("vm_cap_output");
			xo_emit("{:mmap_start_addr/%*s}", ptrwidth, all_vm_info[i].start_addr);
			xo_emit("{:mmap_end_addr/%*s}", ptrwidth, all_vm_info[i].end_addr);
			
			uintptr_t start_addr = (uintptr_t)strtol(all_vm_info[i].start_addr, NULL, 0);
		       	uintptr_t end_addr = (uintptr_t)strtol(all_vm_info[i].end_addr, NULL, 0);
		
			int out_cap_count=0;
			//int in_cap_count=0;
			int ro_count=0;
			int rw_count=0;
			int rx_count=0;

			for (int j=0; j<all_cap_info_index; j++) {
				uintptr_t cap_loc_addr = (uintptr_t)strtol(all_cap_info[j].cap_loc_addr, NULL, 0);
				
				//uintptr_t cap_addr = (uintptr_t)strtol(all_cap_info[j].cap_addr, NULL, 0);
				//if (cap_addr >= start_addr && cap_addr <= end_addr) {
				//	in_cap_count++;
				//}

				if (cap_loc_addr >= start_addr && cap_loc_addr <= end_addr) {
					out_cap_count++;
					if (strchr(all_cap_info[j].perms, 'r') != NULL &&
						strchr(all_cap_info[j].perms, 'w') == NULL) {
						ro_count++;
					}
					if (strchr(all_cap_info[j].perms, 'w') != NULL) {
						rw_count++;
					}
					// TODO: Should this checks for no 'w'?
					if (strchr(all_cap_info[j].perms, 'x') != NULL) {
						rx_count++;
					}
				}
			}
			xo_emit("{:ro_count/%5d} ", ro_count);
			xo_emit("{:rw_count/%5d} ", rw_count);
			xo_emit("{:rx_count/%5d} ", rx_count);

			xo_emit("{:out_cap_count/%8d} ", out_cap_count);

			xo_emit("{:out_cap_density/%8.2f%%} ", ((float)out_cap_count/all_cap_info_index)*100);
			
			xo_emit("{:copy_on_write/%-1s}", all_vm_info[i].mmap_flags &
		    		KVME_FLAG_COW ? "C" : "-");
			xo_emit("{:need_copy/%-1s}", all_vm_info[i].mmap_flags &
		   		KVME_FLAG_NEEDS_COPY ? "N" : "-");
			xo_emit("{:super_pages/%-1s}", all_vm_info[i].mmap_flags &
		 		KVME_FLAG_SUPER ? "S" : "-");
			xo_emit("{:grows_down/%-1s}", all_vm_info[i].mmap_flags &
		    		KVME_FLAG_GROWS_UP ? "U" : all_vm_info[i].mmap_flags &
		    		KVME_FLAG_GROWS_DOWN ? "D" : "-");
			xo_emit("{:wired/%-1s} ", all_vm_info[i].mmap_flags &
		    		KVME_FLAG_USER_WIRED ? "W" : "-");
			
			const char *str, *lstr;	
			switch (all_vm_info[i].vnode_type) {
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

			//char *in_cap_count_str;
			//asprintf(&in_cap_count_str, "%d(%.2f%%)",
			//	in_cap_count, ((float)in_cap_count/all_cap_info_index)*100);
			//xo_emit("{:in_cap_count/%16-s} ", in_cap_count_str);
			//free(in_cap_count_str);
			
			char *filename = (char*)malloc(sizeof(all_vm_info[i].mmap_path));
			get_filename_from_path(all_vm_info[i].mmap_path, &filename);			
			xo_emit("{:mmap_path/%s}\n", filename);
			free(filename);

			free(all_vm_info[i].start_addr);
			free(all_vm_info[i].end_addr);
			free(all_vm_info[i].mmap_path);
		}
		for (int k=0; k<all_cap_info_index; k++) {
			free(all_cap_info[k].cap_loc_addr);
                   	free(all_cap_info[k].cap_addr);
                        free(all_cap_info[k].perms);
		      	free(all_cap_info[k].base);
                        free(all_cap_info[k].top);
		}
		xo_close_instance("vm_cap_output");
	}
	free(all_vm_info);
	free(all_cap_info);

}

/*
 * cap_sym_view
 * SQLite callback routine to traverse results from sqlite_exec()
 * using:
 * 
 * 1. "SELECT * FROM cap_info;"
 * Results are formatted into: cap_loc_addr, cap_addr, perms, base, top
 *
 * 2. "SELECT st_name FROM elf_sym WHERE addr == cap_loc_addr;"
 * Results are added: cap_loc_sym
 *
 * 3. "SELECT st_name FROM elf_sym WHERE addr == cap_addr;"
 * Results are added: cap_sym
 * 
 */
void cap_sym_view(sqlite3 *db, char *lib) {

	/* Obtain how many cap_info items from the database first, we can then use it to 
	 * determine the size of the struct array for holding all the vm entries */
	char *get_cap_count_query;
	asprintf(&get_cap_count_query, "SELECT COUNT(*) FROM cap_info WHERE cap_loc_path LIKE \"%%%s%%\";", lib);
	int rc = sql_query_exec(db, get_cap_count_query, get_cap_info_count_query_callback, NULL); 
	assert(rc == 0);
	free(get_cap_count_query);

	/* Initialise all_cap_info struct to hold the values obtained from the SQL query */
	all_cap_info = calloc(cap_count+1, sizeof(cap_info));
	assert(all_cap_info != NULL);
	
	all_cap_info_index = 0;

	char *get_caps_for_lib_query;
	asprintf(&get_caps_for_lib_query, "SELECT * FROM cap_info WHERE cap_loc_path LIKE \"%%%s%%\";", lib);
	rc &= sql_query_exec(db, get_caps_for_lib_query, cap_info_query_callback, NULL);
	free(get_caps_for_lib_query);

	if (rc == 0) {
		xo_open_instance("cap_sym_output");
		int dbname_len = strlen(get_dbname());
		for (int l=0; l<dbname_len+30; l++) {	
			xo_emit("{:/-}");
		}
		xo_emit("{:/\n%s - total number of caps: %d\n}", get_dbname(), all_cap_info_index);
		for (int l=0; l<dbname_len+30; l++) {	
			xo_emit("{:/-}");
		}

		xo_emit("{T:/\n%17-s %39-s %54-s %10-s}\n",
			"CAP_LOC", "CAP_LOC_SYM", "CAP_INFO", "CAP_SYM");
	
		for (int i=0; i<all_cap_info_index; i++) {
			xo_emit("{:cap_loc/%18s}", all_cap_info[i].cap_loc_addr);
				
			char *get_sym_name_query=NULL;
			char *sym_name_for_cap_loc=NULL;

			asprintf(&get_sym_name_query, "SELECT st_name FROM elf_sym WHERE addr = \"%s\";", all_cap_info[i].cap_loc_addr);
			sql_query_exec(db, get_sym_name_query, sym_name_query_callback, &sym_name_for_cap_loc);
			
			free(get_sym_name_query);

			if (sym_name_for_cap_loc != NULL) {
				xo_emit("{:loc_sym/%40s}", sym_name_for_cap_loc);
			} else {
				xo_emit("{:loc_sym/%40s}", "SYM_NOT_FOUND");
			}
			free(sym_name_for_cap_loc);
		
			char *cap_info = NULL;
			asprintf(&cap_info, "%s (%s,%s-%s)", 
					all_cap_info[i].cap_addr, 
					all_cap_info[i].perms,
					all_cap_info[i].base,
					all_cap_info[i].top);
			xo_emit("{:cap_info/%55s}", cap_info);
	
			free(cap_info);

			char *get_sym_for_cap_query=NULL;
			char *sym_name_for_cap=NULL;
			asprintf(&get_sym_for_cap_query, "SELECT st_name FROM elf_sym WHERE addr = \"%s\";", all_cap_info[i].cap_addr);
			sql_query_exec(db, get_sym_for_cap_query, sym_name_query_callback, &sym_name_for_cap);
			free(get_sym_for_cap_query);
					
			if (sym_name_for_cap != NULL) {
				xo_emit("{:loc_sym/%16-s\n}", sym_name_for_cap);
				free(sym_name_for_cap);
			} else {
				xo_emit("{:loc_sym/%16-s\n}", "SYM_NOT_FOUND");
			}

			free(all_cap_info[i].cap_addr);
			free(all_cap_info[i].perms);
			free(all_cap_info[i].base);
			free(all_cap_info[i].top);

		}
		xo_close_instance("cap_sym_output");
	}

	free(all_cap_info);
}

