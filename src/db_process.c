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
#include <err.h>
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
 * db_table_exists(db, tname)
 * Check if the table with the provided table name tname exists in database db.
 * Returns 0 if the table doesn't exist, as no data can be retrieve for the
 * request, this usually result in program exit.
 * Returns 1 if the table exists.
 */
int db_table_exists(sqlite3 *db, char *tname)
{
	int exists = 0;

	int rc;
	sqlite3_stmt *stmt;
	const char *check_table_q = "SELECT 1 FROM sqlite_master where type='table' and name=?";
	
	rc = sqlite3_prepare_v2(db, check_table_q, -1, &stmt, NULL);

	if (rc != SQLITE_OK) {
		errx(1, "SQL error: %s", sqlite3_errmsg(db));
	}

	sqlite3_bind_text(stmt, 1, tname, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	
	if (rc == SQLITE_ROW) {
		exists = 1;
		debug_print(VERBOSE, "Found %s table in db %s\n", tname, dbname);
	} else if (rc == SQLITE_DONE) {
		exists = 0;
		debug_print(VERBOSE, "%s does not exist in db %s\n", tname, dbname);
	} else {
		sqlite3_finalize(stmt);
		errx(1, "SQL error: %s", sqlite3_errmsg(db));
	}

	sqlite3_finalize(stmt);
	return exists;
}

	
#define assert_db_table_exists(db, tname) {\
	if (0 == db_table_exists(db, tname)) {\
		errx(1, "%s table does not exist on db %s", tname, dbname);\
	}\
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
		"compart_id INTEGER NOT NULL, "
		"kve_protection INTEGER NOT NULL, "
		"mmap_flags INTEGER NOT NULL, "
		"vnode_type INTEGER NOT NULL, "
		//"bss_addr VARCHAR, "
		//"bss_size VARCHAR, "
		"plt_addr VARCHAR, "
		"plt_size VARCHAR, "
		"got_addr VARCHAR, "
		"got_size VARCHAR);";
	
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

	return (0);
}

/*
 * create_compartment_db
 */
int create_compartment_db(sqlite3 *db)
{
	char *compartment_table = 
		"CREATE TABLE IF NOT EXISTS compartment_data("
		"source_start_addr VARCHAR NOT NULL, "
		"source_end_addr VARCHAR NOT NULL, "
		"source_compart_id INTEGER NOT NULL, "
		"source_compart_path VARCHAR NOT NULL, "
		"dest_cap_addr VARCHAR, "
		"dest_cap_perms VARCHAR, "
		"dest_cap_base VARCHAR, "
		"dest_cap_top VARCHAR);";
	
	int rc;
	char* messageError;

	rc = sqlite3_exec(db, compartment_table, NULL, 0, &messageError);
	
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", messageError);
		sqlite3_free(messageError);
		return (1);
	} else {
		debug_print(TROUBLESHOOT, "Database table compartment_table created successfully\n", NULL);
	}

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
	debug_print(TROUBLESHOOT, "sqlite mem used, current: %lld high water: %lld\n", pCurrent, pHighwater);
	
	return (0);
}

vm_info *all_vm_info;   
cap_info *all_cap_info;
sym_info *all_sym_info;

int all_vm_info_index, all_cap_info_index, all_sym_info_index;

/*
 * convert_str_to_int
 * A convenient function to check if we can convert the 
 * provided string to a long integer, and exit the program 
 * early when an invalid value is detected.
 */
static int convert_str_to_int(char *str, char *err_msg)
{
	char *pEnd;
	int int_val = strtol(str, &pEnd, 10);
	if (*pEnd != '\0') {
		fprintf(stderr, "DB processing - Expected an integer, got: %s\n", str);
		errx(1, "%s", err_msg);
	}
	return int_val;
}

static char* _strdup_or_null(char* arg)
{
	if (arg == NULL) {
		return NULL;
	} else {
		return strdup(arg);
	}
}

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
        /* Database schema for vm has 11 columns */
        assert(argc == 11);
 
        vm_info vm_info_captured;
	int i=0;

 	vm_info_captured.start_addr = strdup(argv[i++]);
        vm_info_captured.end_addr = strdup(argv[i++]);
        vm_info_captured.mmap_path = strdup(argv[i++]);
	vm_info_captured.compart_id = convert_str_to_int(argv[i++], "compart_id is invalid");

	// Checking that the received values are valid integers
	// as expected from the vm_info_captured data structure. 
	// If the check fails, it means the data is invalid. Without
	// a more sophisticated logic in place to handle invalid data, 
	// we will just quit the program and let users to examine 
	// and fix the data before running this function again.
	vm_info_captured.kve_protection = convert_str_to_int(argv[i++], "kve_protection is invalid");
     	vm_info_captured.mmap_flags = convert_str_to_int(argv[i++], "mmap_flags is invalid"); 
        vm_info_captured.vnode_type = convert_str_to_int(argv[i++], "vnode_type is invalid");

        vm_info_captured.plt_addr = _strdup_or_null(argv[i++]);
        vm_info_captured.plt_size = _strdup_or_null(argv[i++]);
        vm_info_captured.got_addr = _strdup_or_null(argv[i++]);
        vm_info_captured.got_size = _strdup_or_null(argv[i++]);

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

	assert_db_table_exists(db, "vm");

        /* Obtain how many vm items from the database first, we can then use it to
         * determine the size of the struct array for holding all the vm entries */
        char *count;
        sql_query_exec(db, "SELECT COUNT(*) FROM vm;", vm_info_count_query_callback, &count);
	int result_count = convert_str_to_int(count, "Query to get count from vm returned an invalid value");
	free(count);
	return result_count;
}

int cap_info_count(sqlite3 *db)
{
	assert_db_table_exists(db, "cap_info");

        /* Obtain how many vm items from the database first, we can then use it to
         * determine the size of the struct array for holding all the vm entries */
        char *count;
	sql_query_exec(db, "SELECT COUNT(*) FROM cap_info;", cap_info_count_query_callback, &count);
	int result_count = convert_str_to_int(count, "Query to get count from cap_info returned an invalid value");
	free(count);
	return result_count;
}

int sym_info_count(sqlite3 *db)
{
	assert_db_table_exists(db, "elf_sym");

        /* Obtain how many vm items from the database first, we can then use it to
         * determine the size of the struct array for holding all the vm entries */
        char *count;
	sql_query_exec(db, "SELECT COUNT(*) FROM elf_sym;", sym_info_count_query_callback, &count);
	int result_count = convert_str_to_int(count, "Query to get count from elf_sym returned an invalid value");
	free(count);
	return result_count;
}

int cap_info_for_lib_count(sqlite3 *db, char *lib)
{      
	assert_db_table_exists(db, "cap_info");

        /* Obtain how many vm items from the database first, we can then use it to
         * determine the size of the struct array for holding all the vm entries */
        char *count;

	char *query;
	asprintf(&query, "SELECT COUNT(*) FROM cap_info WHERE cap_loc_path LIKE \"%%%s%%\";", lib);
        sql_query_exec(db, query, cap_info_count_query_callback, &count);
	int result_count = convert_str_to_int(count, "Query to get count from cap_info returned an invalid value");
        free(query);
	free(count);
        return result_count;
}

int get_all_vm_info(sqlite3 *db, vm_info **all_vm_info_ptr)
{
	assert_db_table_exists(db, "vm");

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
	assert_db_table_exists(db, "cap_info");

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
	assert_db_table_exists(db, "elf_sym");

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
	assert_db_table_exists(db, "cap_info");

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

