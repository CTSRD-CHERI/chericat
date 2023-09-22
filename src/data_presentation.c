#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxo/xo.h>

#include "db_process.h"

typedef struct vm_info_struct {
	char *start_addr;
	char *end_addr;
	char *mmap_path;
} vm_info;

typedef struct cap_info_struct {
	char *cap_self_addr;
	char *cap_addr;
	char *perms;
	char *base;
	char *top;
} cap_info;

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
	/* Database schema for vm has 4 columns */
	assert(argc == 4);

	vm_info vm_info;
	vm_info.start_addr = strdup(argv[0]);
	vm_info.end_addr = strdup(argv[1]);
	vm_info.mmap_path = strdup(argv[2]);

	all_vm_info[all_vm_info_index++] = vm_info;
	
    	return 0;
}

/*
 * caps_query_callback
 * SQLite callback routine to traverse results from sqlite_exec()
 * using: "SELECT * FROM cap_info;"
 * Results are formatted into: cap_self_addr, cap_addr, perms, base, top
 *
 * argc - number of columns
 * argv - pointer to the data that is stored in each column
 * azColName - pointer to the colunmn names
 */
static int cap_info_query_callback(void *NotUsed, int argc, char **argv, char **azColName)
{
	assert(argc == 5);
	
	cap_info cap_info;
	cap_info.cap_self_addr = strdup(argv[0]);
	cap_info.cap_addr = strdup(argv[1]);
	cap_info.perms = strdup(argv[2]);
	cap_info.base = strdup(argv[3]);
	cap_info.top = strdup(argv[4]);

	all_cap_info[all_cap_info_index++] = cap_info;
    	
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
 * 2. "SELECT * FROM cap_info WHERE cap_self_addr >= start_addr AND cap_self_addr <= end_addr;"
 * Results are added: no_of_caps(%), no_of_ro_caps, no_of_rw_caps, no_of_x_caps
 * 
 */
void vm_caps_view() {
	/* Obtain how many vm items from the database first, we can then use it to 
	 * determine the size of the struct array for holding all the vm entries */
	char *get_vm_count_query = strdup("SELECT COUNT(*) FROM vm;");
	int rc = sql_query_exec(get_vm_count_query, get_vm_count_query_callback); 
	assert(rc == 0);
	free(get_vm_count_query);

	/* Initialise all_vm_info struct to hold the values obtained from SQL query */
	all_vm_info = calloc(vm_count+1, sizeof(vm_info));
	assert (all_vm_info != NULL);
	
	all_vm_info_index = 0;
		
	char *select_vm_query = strdup("SELECT * FROM vm;");          
	rc = rc & sql_query_exec(select_vm_query, vm_query_callback);

	/* Obtain how many cap_info items from the database first, we can then use it to 
	 * determine the size of the struct array for holding all the vm entries */
	char *get_cap_info_count_query = strdup("SELECT COUNT(*) FROM cap_info;");
	rc = rc & sql_query_exec(get_cap_info_count_query, get_cap_info_count_query_callback); 
	assert(rc == 0);
	free(get_cap_info_count_query);

	/* Initialise all_cap_info struct to hold the values obtained from the SQL query */
	all_cap_info = calloc(cap_count+1, sizeof(cap_info));
	assert(all_cap_info != NULL);
	
	all_cap_info_index = 0;

	char *select_cap_query = strdup("SELECT * FROM cap_info;");
	rc &= sql_query_exec(select_cap_query, cap_info_query_callback);

	if (rc == 0) {
		xo_open_instance("vm_cap_output");
		int dbname_len = strlen(get_dbname());
		for (int l=0; l<dbname_len+4; l++) {	
			xo_emit("{:/=}");
		}
		xo_emit("{:/\n* %s *\n}", get_dbname());
		for (int l=0; l<dbname_len+4; l++) {	
			xo_emit("{:/=}");
		}

		xo_emit("{T:/\n%28-s %20-s %16-s %16-s %16-s %5s %5s %5s}\n",
			"PATH", "START", "END", "IN CAP(RATIO)", "OUT CAP(RATIO)", "R", "W", "X");
	
		for (int i=0; i<all_vm_info_index; i++) {
			char *filename = (char*)malloc(sizeof(all_vm_info[i].mmap_path));
			get_filename_from_path(all_vm_info[i].mmap_path, &filename);
			xo_emit("{:mmap_path/%28-s} ", filename);

			xo_emit("{:mmap_start_addr/%20-s} ", all_vm_info[i].start_addr);
			xo_emit("{:mmap_end_addr/%16-s} ", all_vm_info[i].end_addr);
			
			uintptr_t start_addr = (uintptr_t)strtol(all_vm_info[i].start_addr, NULL, 0);
		       	uintptr_t end_addr = (uintptr_t)strtol(all_vm_info[i].end_addr, NULL, 0);
		
			int out_cap_count=0;
			int in_cap_count=0;
			int r_count=0;
			int w_count=0;
			int x_count=0;

			for (int j=0; j<all_cap_info_index; j++) {
				uintptr_t cap_self_addr = (uintptr_t)strtol(all_cap_info[j].cap_self_addr, NULL, 0);
				uintptr_t cap_addr = (uintptr_t)strtol(all_cap_info[j].cap_addr, NULL, 0);

				if (cap_addr >= start_addr && cap_addr <= end_addr) {
					in_cap_count++;
				}

				if (cap_self_addr >= start_addr && cap_self_addr <= end_addr) {
					out_cap_count++;
					if (strchr(all_cap_info[j].perms, 'r') != NULL) {
						r_count++;
					}
					if (strchr(all_cap_info[j].perms, 'w') != NULL) {
						w_count++;
					}
					if (strchr(all_cap_info[j].perms, 'x') != NULL) {
						x_count++;
					}
				}
			}
			char *out_cap_count_str;
			asprintf(&out_cap_count_str, "%d(%.2f%%)", 
				out_cap_count, ((float)out_cap_count/all_cap_info_index)*100);

			char *in_cap_count_str;
			asprintf(&in_cap_count_str, "%d(%.2f%%)",
				in_cap_count, ((float)in_cap_count/all_cap_info_index)*100);

			xo_emit("{:in_cap_count/%16-s} ", in_cap_count_str);
			xo_emit("{:out_cap_count/%16-s} ", out_cap_count_str);
			xo_emit("{:r_count/%5d} ", r_count);
			xo_emit("{:w_count/%5d} ", w_count);
			xo_emit("{:x_count/%5d}\n", x_count);

			free(in_cap_count_str);
			free(out_cap_count_str);
			free(all_vm_info[i].start_addr);
			free(all_vm_info[i].end_addr);
			free(all_vm_info[i].mmap_path);
		}
		for (int k=0; k<all_cap_info_index; k++) {
			free(all_cap_info[k].cap_self_addr);
                   	free(all_cap_info[k].cap_addr);
                        free(all_cap_info[k].perms);
		      	free(all_cap_info[k].base);
                        free(all_cap_info[k].top);
		}
		free(select_vm_query);
		free(all_vm_info);
		free(select_cap_query);
		free(all_cap_info);
		xo_close_instance("vm_cap_output");
	}

}

