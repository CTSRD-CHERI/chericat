#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sqlite3.h>

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <cheri/cheric.h>

#include "db_process.h"
#include "common.h"

char *dbname;

char *get_dbname() {
	if (dbname == NULL) {
		dbname = strdup(":memory:");
	}
	return dbname;
}

/*
 * create_vm_cap_db
 * Creates two tables, one for the VM entries and the other one contains all the 
 * discovered capabilities and their info
 * For each VM entry there is a reference to the capability addresses within the block.
 */
int create_vm_cap_db(sqlite3 *db)
{
	char *vm_table = 
		"CREATE TABLE IF NOT EXISTS vm("
		"start_addr VARCHAR NOT NULL, "
		"end_addr VARCHAR NOT NULL, "
		"mmap_path VARCHAR NOT NULL, "
		"kve_protection INTEGER NOT NULL, "
		"mmap_flags INTEGER NOT NULL, "
		"vnode_type INTEGER NOT NULL);";
	
	char *cap_info_table =
		"CREATE TABLE IF NOT EXISTS cap_info("
		"cap_loc_addr VARCHAR NOT NULL, "
		"cap_loc_path VARCHAR NOT NULL, "
		"cap_addr VARCHAR NOT NULL, "
		"perms VARCHAR NOT NULL, "
		"base VARCHAR NOT NULL, "
		"top VARCHAR NOT NULL);";

	int rc;
	char* messageError;

	rc = sqlite3_exec(db, cap_info_table, NULL, 0, &messageError);
	
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", messageError);
		sqlite3_free(messageError);
		return (1);
	} else {
		debug_print(TROUBLESHOOT, "Database table cap_info_table created successfully\n", NULL);
	}

	rc = sqlite3_exec(db, vm_table, NULL, 0, &messageError);

	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", messageError);
		sqlite3_free(messageError);
		return (1);
	} else {
		debug_print(TROUBLESHOOT, "Database table vm_table created successfully\n", NULL);
	}
	
	/*
	sqlite3_int64 pCurrent=0, pHighwater=0;
	sqlite3_status64(SQLITE_STATUS_MEMORY_USED, &pCurrent, &pHighwater, 0);
	debug_print(VERBOSE, "sqlite mem used, current: %lld high water: %lld\n", pCurrent, pHighwater);
	*/
	return (0);
}

/*
 * create_elf_sym_db
 */
int create_elf_sym_db(sqlite3 *db)
{
	char *elf_sym_table = 
		"CREATE TABLE IF NOT EXISTS elf_sym("
		"source_path VARCHAR NOT NULL, "
		"st_name VARCHAR NOT NULL, "
		"st_value VARCHAR NOT NULL, "
		"st_shndx VARCHAR NOT NULL, "
		"type VARCHAR NOT NULL, "
		"bind VARCHAR NOT NULL, "
		"addr VARCHAR NOT NULL);";
	
	int rc;
	char* messageError;

	rc = sqlite3_exec(db, elf_sym_table, NULL, 0, &messageError);
	
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", messageError);
		sqlite3_free(messageError);
		return (1);
	} else {
		debug_print(TROUBLESHOOT, "Database table elf_sym_table created successfully\n", NULL);
	}

	/*
	sqlite3_int64 pCurrent=0, pHighwater=0;
	sqlite3_status64(SQLITE_STATUS_MEMORY_USED, &pCurrent, &pHighwater, 0);
	debug_print(VERBOSE, "sqlite mem used, current: %lld high water: %lld\n", pCurrent, pHighwater);
	*/
	return (0);
}

int begin_transaction(sqlite3 *db)
{
	int rc;
	char *messageError;

	rc = sqlite3_exec(db, "BEGIN", 0, 0, &messageError);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", messageError);
		sqlite3_free(messageError);
		//sqlite3_close(db);
		return (1);
	}

	return (0);
}

int commit_transaction(sqlite3 *db)
{
	int rc;
	char *messageError;

	rc = sqlite3_exec(db, "COMMIT", 0, 0, &messageError);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", messageError);
		sqlite3_free(messageError);
		//sqlite3_close(db);
		return (1);
	}

	return (0);
}	

int sql_query_exec(sqlite3 *db, char* query, int (*callback)(void*,int,char**,char**), void *data)
{
	int rc;
	char* messageError;

	rc = sqlite3_exec(db, query, callback, data, &messageError);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s (db: %s)\n", messageError, get_dbname());
		sqlite3_free(messageError);
		return (1);
	} else {
		debug_print(TROUBLESHOOT, "Query %s, executed successfully (rc=%d) \n", query, rc);
	}
	
	sqlite3_int64 pCurrent=0, pHighwater=0;
	sqlite3_status64(SQLITE_STATUS_MEMORY_USED, &pCurrent, &pHighwater, 0);
	debug_print(VERBOSE, "sqlite mem used, current: %lld high water: %lld\n", pCurrent, pHighwater);
	
	return (0);
}

vm_info *all_vm_info;   
cap_info *all_cap_info;
sym_info *all_sym_info;

int all_vm_info_index, all_cap_info_index, all_sym_info_index;

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
static int vm_info_query_callback(void *all_vm_info_ptr, int argc, char **argv, char **azColName)
{
        /* Database schema for vm has 6 columns */
        assert(argc == 6);
 
        vm_info vm_info_captured;
        vm_info_captured.start_addr = strdup(argv[0]);
        vm_info_captured.end_addr = strdup(argv[1]);
        vm_info_captured.mmap_path = strdup(argv[2]);
     	vm_info_captured.kve_protection = atoi(argv[3]);
     	vm_info_captured.mmap_flags = atoi(argv[4]); 
        vm_info_captured.vnode_type = atoi(argv[5]);

	vm_info **result_ptr = (vm_info **)all_vm_info_ptr;
       	(*result_ptr)[all_vm_info_index++] = vm_info_captured;

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
static int cap_info_query_callback(void *all_cap_info_ptr, int argc, char **argv, char **azColName)
{
        assert(argc == 6);

        cap_info cap_info_captured;
        cap_info_captured.cap_loc_addr = strdup(argv[0]);
        cap_info_captured.cap_loc_path = strdup(argv[1]);
        cap_info_captured.cap_addr = strdup(argv[2]);
        cap_info_captured.perms = strdup(argv[3]);
        cap_info_captured.base = strdup(argv[4]);
        cap_info_captured.top = strdup(argv[5]);

	cap_info **result_ptr = (cap_info **)all_cap_info_ptr;
	(*result_ptr)[all_cap_info_index++] = cap_info_captured;

        return 0;
}

static int sym_info_query_callback(void *all_sym_info_ptr, int argc, char **argv, char **azColName)
{
        assert(argc == 7);

	sym_info sym_info_captured;
        sym_info_captured.source_path = strdup(argv[0]);
        sym_info_captured.sym_name = strdup(argv[1]);
        sym_info_captured.sym_offset = strdup(argv[2]);
        sym_info_captured.shndx = strdup(argv[3]);
        sym_info_captured.type = strdup(argv[4]);
        sym_info_captured.bind = strdup(argv[5]);
        sym_info_captured.addr = strdup(argv[6]);

	sym_info **result_ptr = (sym_info **)all_sym_info_ptr;
	(*result_ptr)[all_sym_info_index++] = sym_info_captured;

	return 0;
}

static int vm_info_count_query_callback(void *count, int argc, char **argv, char **azColName)
{
	assert(argc > 0);
	char **result_ptr = (char **)count;
	*result_ptr = strdup(argv[0]);
    	return 0;
}

static int cap_info_count_query_callback(void *count, int argc, char **argv, char **azColName)
{
	assert(argc > 0);
	char **result_ptr = (char **)count;
	*result_ptr = strdup(argv[0]);
	return 0;
}

static int sym_info_count_query_callback(void *count, int argc, char **argv, char **azColName)
{
	assert(argc > 0);
	char **result_ptr = (char **)count;
	*result_ptr = strdup(argv[0]);
	return 0;
}

int vm_info_count(sqlite3 *db) 
{
        /* Obtain how many vm items from the database first, we can then use it to
         * determine the size of the struct array for holding all the vm entries */
        char *count;
        sql_query_exec(db, "SELECT COUNT(*) FROM vm;", vm_info_count_query_callback, &count);

	int result_count = atoi(count);
	free(count);
	return result_count;
}

int cap_info_count(sqlite3 *db)
{
        /* Obtain how many vm items from the database first, we can then use it to
         * determine the size of the struct array for holding all the vm entries */
        char *count;
	sql_query_exec(db, "SELECT COUNT(*) FROM cap_info;", cap_info_count_query_callback, &count);

        int result_count = atoi(count);
	free(count);
	return result_count;
}

int sym_info_count(sqlite3 *db)
{
        /* Obtain how many vm items from the database first, we can then use it to
         * determine the size of the struct array for holding all the vm entries */
        char *count;
	sql_query_exec(db, "SELECT COUNT(*) FROM elf_sym;", sym_info_count_query_callback, &count);

        int result_count = atoi(count);
	free(count);
	return result_count;
}

int cap_info_for_lib_count(sqlite3 *db, char *lib)
{       
        /* Obtain how many vm items from the database first, we can then use it to
         * determine the size of the struct array for holding all the vm entries */
        char *count;

	char *query;
	asprintf(&query, "SELECT COUNT(*) FROM cap_info WHERE cap_loc_path LIKE \"%%%s%%\";", lib);
        sql_query_exec(db, query, cap_info_count_query_callback, &count);
        
        int result_count = atoi(count);
        free(query);
	free(count);
        return result_count;
}

int get_all_vm_info(sqlite3 *db, vm_info **all_vm_info_ptr)
{
	int vm_count = vm_info_count(db);
        *all_vm_info_ptr = (vm_info *)calloc(vm_count, sizeof(vm_info));
        assert (*all_vm_info_ptr != NULL);
        
        int rc = sql_query_exec(db, "SELECT * FROM vm;", vm_info_query_callback, all_vm_info_ptr);

	if (rc == 0) {
		return vm_count;
	} else {
		return -1;
	}
}

int get_all_cap_info(sqlite3 *db, cap_info **all_cap_info_ptr)
{

	int cap_count = cap_info_count(db);
        *all_cap_info_ptr = (cap_info *)calloc(cap_count, sizeof(cap_info));
        assert (*all_cap_info_ptr != NULL);
        
        int rc = sql_query_exec(db, "SELECT * FROM cap_info;", cap_info_query_callback, all_cap_info_ptr);

	if (rc == 0) {
		return cap_count;
	} else {
		return -1;
	}
}

int get_all_sym_info(sqlite3 *db, sym_info **all_sym_info_ptr)
{
	int sym_count = sym_info_count(db);
        *all_sym_info_ptr = (sym_info *)calloc(sym_count, sizeof(sym_info));
        assert (*all_sym_info_ptr != NULL);
        
        int rc = sql_query_exec(db, "SELECT * FROM elf_sym;", sym_info_query_callback, all_sym_info_ptr);

	if (rc == 0) {
		return sym_count;
	} else {
		return -1;
	}
}

int get_cap_info_for_lib(sqlite3 *db, cap_info **cap_info_captured_ptr, char *lib)
{
	int cap_count = cap_info_for_lib_count(db, lib);
	*cap_info_captured_ptr = (cap_info *)calloc(cap_count, sizeof(cap_info));
	assert (*cap_info_captured_ptr != NULL);

	char *query;
	asprintf(&query, "SELECT * FROM cap_info WHERE cap_loc_path LIKE \"%%%s%%\";", lib);
	int rc = sql_query_exec(db, query, cap_info_query_callback, cap_info_captured_ptr);

	free(query);

	if (rc == 0) {
		return cap_count;
	} else {
		return -1;
	}
}

void db_info_capture_test()
{
	printf("Testing Testing\n");

	sqlite3 *db;
	dbname = strdup("/home/psjm3/chericat/trial.db");
	
	int rc = sqlite3_open(dbname, &db);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Error open DB %s", sqlite3_errmsg(db));
		sqlite3_close(db);
	}

	printf("vm_info_count obtained: %d\n", vm_info_count(db));
	printf("cap_info_count obtained: %d\n", cap_info_count(db));
	printf("sym_info_count obtained: %d\n", sym_info_count(db));

	vm_info *vm_info_captured;
	int vm_count = get_all_vm_info(db, &vm_info_captured);
	assert(vm_count != -1);

	for (int i=0; i<vm_count; i++) {
		printf("vm_info_captured[%d]:\n", i); 
		printf("     %s\n", vm_info_captured[i].start_addr);
		printf("     %s\n", vm_info_captured[i].end_addr);
		printf("     %s\n", vm_info_captured[i].mmap_path);
		printf("     %d\n", vm_info_captured[i].kve_protection);
	}
	
	cap_info *cap_info_captured;
	int cap_count = get_all_cap_info(db, &cap_info_captured);
	assert(cap_count != -1);

	for (int i=0; i<10; i++) {
		printf("cap_info_captured[%d]:\n", i); 
		printf("     %s\n", cap_info_captured[i].cap_loc_addr);
		printf("     %s\n", cap_info_captured[i].cap_loc_path);
		printf("     %s\n", cap_info_captured[i].cap_addr);
	}
	
	sym_info *sym_info_captured;
	int sym_count = get_all_sym_info(db, &sym_info_captured);
	assert(sym_count != -1);

	for (int i=0; i<10; i++) {
		printf("sym_info_captured[%d]:\n", i); 
		printf("     %s\n", sym_info_captured[i].source_path);
		printf("     %s\n", sym_info_captured[i].sym_name);
		printf("     %s\n", sym_info_captured[i].addr);
	}
}

