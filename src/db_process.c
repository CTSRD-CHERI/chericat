#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sqlite3.h>

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <cheri/cheric.h>

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
	//sqlite3* db;
	int exit = 0;

	char *vm_table = 
		"CREATE TABLE IF NOT EXISTS vm("
		"start_addr VARCHAR NOT NULL, "
		"end_addr VARCHAR NOT NULL, "
		"mmap_path VARCHAR NOT NULL, "
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

	/*
	exit = sqlite3_open(get_dbname(), &db);

	if (exit) {
		fprintf(stderr, "Error open DB %s", sqlite3_errmsg(db));
		//sqlite3_close(db);
		return (1);
	} else {
		debug_print(TROUBLESHOOT, "Database open successfully\n", NULL);
	}
	*/

	int rc;
	char* messageError;

	rc = sqlite3_exec(db, cap_info_table, NULL, 0, &messageError);
	
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", messageError);
		sqlite3_free(messageError);
		//sqlite3_close(db);
		return (1);
	} else {
		debug_print(TROUBLESHOOT, "Database table cap_info_table created successfully\n", NULL);
	}

	rc = sqlite3_exec(db, vm_table, NULL, 0, &messageError);

	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", messageError);
		sqlite3_free(messageError);
		//sqlite3_close(db);
		return (1);
	} else {
		debug_print(TROUBLESHOOT, "Database table vm_table created successfully\n", NULL);
	}
	
	/*
	sqlite3_int64 pCurrent=0, pHighwater=0;
	sqlite3_status64(SQLITE_STATUS_MEMORY_USED, &pCurrent, &pHighwater, 0);
	debug_print(VERBOSE, "sqlite mem used, current: %lld high water: %lld\n", pCurrent, pHighwater);
	*/
	//sqlite3_close(db);
	return (0);
}

/*
 * create_elf_sym_db
 */
int create_elf_sym_db(sqlite3 *db)
{
	//sqlite3* db;
	int exit = 0;

	char *elf_sym_table = 
		"CREATE TABLE IF NOT EXISTS elf_sym("
		"source_path VARCHAR NOT NULL, "
		"st_name VARCHAR NOT NULL, "
		"st_value VARCHAR NOT NULL, "
		"st_shndx VARCHAR NOT NULL, "
		"type VARCHAR NOT NULL, "
		"bind VARCHAR NOT NULL, "
		"addr VARCHAR NOT NULL);";
	
	/*
	exit = sqlite3_open(get_dbname(), &db);

	if (exit) {
		fprintf(stderr, "Error open DB %s", sqlite3_errmsg(db));
		sqlite3_close(db);
		return (1);
	} else {
		debug_print(TROUBLESHOOT, "Database %s open successfully\n", get_dbname());
	}
	*/

	int rc;
	char* messageError;

	rc = sqlite3_exec(db, elf_sym_table, NULL, 0, &messageError);
	
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", messageError);
		sqlite3_free(messageError);
		//sqlite3_close(db);
		return (1);
	} else {
		debug_print(TROUBLESHOOT, "Database table elf_sym_table created successfully\n", NULL);
	}

	/*
	sqlite3_int64 pCurrent=0, pHighwater=0;
	sqlite3_status64(SQLITE_STATUS_MEMORY_USED, &pCurrent, &pHighwater, 0);
	debug_print(VERBOSE, "sqlite mem used, current: %lld high water: %lld\n", pCurrent, pHighwater);
	*/
	//sqlite3_close(db);
	return (0);
}

int begin_transaction(sqlite3 *db)
{
	/*
	sqlite3* db;
	int exit = 0;

	exit = sqlite3_open(get_dbname(), &db);

	if (exit) {
		fprintf(stderr, "Error open DB %s", sqlite3_errmsg(db));
		//sqlite3_close(db);
		return (1);
	} else {
		debug_print(TROUBLESHOOT, "Database open successfully\n", NULL);
	}
	*/

	int rc;
	char *messageError;

	rc = sqlite3_exec(db, "BEGIN", 0, 0, &messageError);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", messageError);
		sqlite3_free(messageError);
		//sqlite3_close(db);
		return (1);
	}

	//sqlite3_close(db);
	return (0);
}

int commit_transaction(sqlite3 *db)
{
	/*
	sqlite3* db;
	int exit = 0;

	exit = sqlite3_open(get_dbname(), &db);

	if (exit) {
		fprintf(stderr, "Error open DB %s", sqlite3_errmsg(db));
		sqlite3_close(db);
		return (1);
	} else {
		debug_print(TROUBLESHOOT, "Database open successfully\n", NULL);
	}
	*/

	int rc;
	char *messageError;

	rc = sqlite3_exec(db, "COMMIT", 0, 0, &messageError);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", messageError);
		sqlite3_free(messageError);
		//sqlite3_close(db);
		return (1);
	}

	//sqlite3_close(db);
	return (0);
}	

int sql_query_exec(sqlite3 *db, char* query, int (*callback)(void*,int,char**,char**), char **data)
{
	/*
	sqlite3* db;
	int exit = 0;

	exit = sqlite3_open(get_dbname(), &db);

	if (exit) {
		fprintf(stderr, "Error open DB %s", sqlite3_errmsg(db));
		sqlite3_close(db);
		return (1);
	} else {
		debug_print(TROUBLESHOOT, "Database open successfully\n", NULL);
	}
	*/

	int rc;
	char* messageError;

	/*
	rc = sqlite3_exec(db, "PRAGMA journal_mode = WAL", 0, 0, 0);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", messageError);
		sqlite3_free(messageError);
		return (1);
	} else {
		debug_print(VERBOSE, "Query %s, executed successfully (rc=%d) \n", query, rc);
	}

	rc = sqlite3_exec(db, "PRAGMA synchronous = NORMAL", 0, 0, 0);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", messageError);
		sqlite3_free(messageError);
		return (1);
	} else {
		debug_print(VERBOSE, "Query %s, executed successfully (rc=%d) \n", query, rc);
	}
	*/

	rc = sqlite3_exec(db, query, callback, data, &messageError);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s (db: %s)\n", messageError, get_dbname());
		sqlite3_free(messageError);
		//sqlite3_close(db);
		return (1);
	} else {
		debug_print(TROUBLESHOOT, "Query %s, executed successfully (rc=%d) \n", query, rc);
	}
	
	sqlite3_int64 pCurrent=0, pHighwater=0;
	sqlite3_status64(SQLITE_STATUS_MEMORY_USED, &pCurrent, &pHighwater, 0);
	debug_print(VERBOSE, "sqlite mem used, current: %lld high water: %lld\n", pCurrent, pHighwater);
	
	//sqlite3_close(db);
	return (0);
}

