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
#include <errno.h>
#include <string.h>
#include <sqlite3.h>

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <cheri/cheric.h>

#include "db_process.h"
#include "common.h"

#define numeric_test_failed(expected, obtained, errmsg) {\
	fprintf(stderr, "%s:%s:%d: Expected: %d Obtained: %d Error Message: %s\n", __FILE__, __func__, __LINE__, expected, obtained, errmsg);\
}
#define string_test_failed(expected, obtained, errmsg) {\
	fprintf(stderr, "%s:%s:%d: Expected: %s Obtained: %s Error Message: %s\n", __FILE__, __func__, __LINE__, expected, obtained, errmsg);\
}

static char *testdb = "chericat-test.db";

static int check_if_sqlite_ok(sqlite3 *db, int rc, char *err_msg)
{
	if (rc != SQLITE_OK) {
		char *err_msg_combined;
		asprintf(&err_msg_combined, "SQL error: %s", err_msg);
		numeric_test_failed(SQLITE_OK, rc, err_msg_combined);
		sqlite3_free(err_msg);
		free(err_msg_combined);
		sqlite3_close(db);
		exit(1);
	}
	return 0;
}

static int prepare_db(sqlite3 *db)
{
	char *err_msg;
	char *drop_table_query =
		"DROP TABLE IF EXISTS vm;"
		"DROP TABLE IF EXISTS cap_info;"
		"DROP TABLE IF EXISTS elf_sym;";
	int rc = sqlite3_exec(db, drop_table_query, 0, 0, &err_msg);
	check_if_sqlite_ok(db, rc, err_msg);
	return 0;
}

static int create_test_tables(sqlite3 *db)
{
	int ret;
	ret = create_vm_cap_db(db);
	ret = ret | create_elf_sym_db(db);
	return ret;
}

static int insert_test_data(sqlite3 *db)
{
	char *err_msg;
	char *insert_query = 
		"INSERT INTO vm VALUES(\"0x100000\",\"0x101000\",\"/usr/home/psjm3/exercises/print-pointer\", 33, 7, 2, \"0x131180\", \"0x40\", \"0x110d50\", \"0x90\", \"0x120fc0\", \"0xe0\");"
		"INSERT INTO cap_info VALUES(\"0x120de0\",\"/usr/home/psjm3/exercises/print-pointer(.got)\",\"0x110c21\",\"rxRE\",\"0x100000\",\"0x1311c0\");"
		"INSERT INTO elf_sym VALUES(\"/usr/home/psjm3/exercises/print-pointer\", \"atexit\", \"0x0\", \"UND\", \"FUNC\", \"GLOBAL\", \"0x100000\");";

	int rc = sqlite3_exec(db, insert_query, 0, 0, &err_msg);
	check_if_sqlite_ok(db, rc, err_msg);
	return 0;
}

int vm_info_count_test(sqlite3 *db) 
{
	int result = vm_info_count(db);
	if (result != 1) {
		numeric_test_failed(1, result, "");
		return 1;
	}
	return 0;
}

int cap_info_count_test(sqlite3 *db)
{
	int result = cap_info_count(db);
        if (result != 1) {
		numeric_test_failed(1, result, "");
                return 1;
        }
        return 0;
}

int sym_info_count_test(sqlite3 *db)
{
	int result = sym_info_count(db);
	if (result != 1) {
		numeric_test_failed(1, result, "");
		return 1;
	}
	return 0;
}

int cap_info_for_lib_count_test(sqlite3 *db)
{
	int result = cap_info_for_lib_count(db, "print-pointer");
	if (result != 1) {
		numeric_test_failed(1, result, "");
		return 1;
	}
	return 0;
}

int get_all_vm_info_test(sqlite3 *db)
{
	vm_info *vm_info_captured;
	int vm_count = get_all_vm_info(db, &vm_info_captured);
	if (vm_count != 1) {
		numeric_test_failed(1, vm_count, "");
		return 1;
	}
	if (strcmp(vm_info_captured[0].start_addr, "0x100000") != 0) {
		string_test_failed("0x100000", vm_info_captured[0].start_addr, "");
		return 1;
	}
	if (strcmp(vm_info_captured[0].end_addr, "0x101000") != 0) {
		string_test_failed("0x101000", vm_info_captured[0].end_addr, "");
		return 1;
	}
	if (strcmp(vm_info_captured[0].mmap_path, "/usr/home/psjm3/exercises/print-pointer") != 0) {
		string_test_failed("/usr/home/psjm3/exercises/print-pointer", vm_info_captured[0].mmap_path, "");
		return 1;
	}
	if (vm_info_captured[0].kve_protection != 33) {
		numeric_test_failed(33, vm_info_captured[0].kve_protection, "");
		return 1;
	}
	if (vm_info_captured[0].mmap_flags != 7) {
		numeric_test_failed(7, vm_info_captured[0].mmap_flags, "");
		return 1;
	}
	if (vm_info_captured[0].vnode_type != 2) {
		numeric_test_failed(2, vm_info_captured[0].vnode_type, "");
		return 1;
	}
	if (strcmp(vm_info_captured[0].bss_addr, "0x131180") != 0) {
		string_test_failed("0x131180", vm_info_captured[0].bss_addr, "");
		return 1;
	}
	if (strcmp(vm_info_captured[0].bss_size, "0x40") != 0) {
		string_test_failed("0x40", vm_info_captured[0].bss_size, "");
		return 1;
	}
	if (strcmp(vm_info_captured[0].plt_addr, "0x110d50") != 0) {
		string_test_failed("0x110d50", vm_info_captured[0].plt_addr, "");
		return 1;
	}
	if (strcmp(vm_info_captured[0].plt_size, "0x90") != 0) {
		string_test_failed("0x90", vm_info_captured[0].plt_size, "");
		return 1;
	}
	if (strcmp(vm_info_captured[0].got_addr, "0x120fc0") != 0) {
		string_test_failed("0x120fc0", vm_info_captured[0].got_addr, "");
		return 1;
	}
	if (strcmp(vm_info_captured[0].got_size, "0xe0") != 0) {
		string_test_failed("0xe0", vm_info_captured[0].got_size, "");
		return 1;
	}
	
	free(vm_info_captured);
	return 0;
}

int get_all_cap_info_test(sqlite3 *db)
{
	cap_info *cap_info_captured;
	int cap_count = get_all_cap_info(db, &cap_info_captured);
	if (cap_count != 1) {
		numeric_test_failed(1, cap_count, "");
		return 1;
	}
	if (strcmp(cap_info_captured[0].cap_loc_addr, "0x120de0") != 0) {
		string_test_failed("0x120de0", cap_info_captured[0].cap_loc_addr, "");
		return 1;
	}
	if (strcmp(cap_info_captured[0].cap_loc_path, "/usr/home/psjm3/exercises/print-pointer(.got)") != 0) {
		string_test_failed("/usr/home/psjm3/exercises/print-pointer(.got)", cap_info_captured[0].cap_loc_path, "");
		return 1;
	}
	if (strcmp(cap_info_captured[0].cap_addr, "0x110c21") != 0) {
		string_test_failed("0x110c21", cap_info_captured[0].cap_addr, "");
		return 1;
	}
	if (strcmp(cap_info_captured[0].perms, "rxRE") != 0) {
		string_test_failed("rxRE", cap_info_captured[0].perms, "");
		return 1;
	}
	if (strcmp(cap_info_captured[0].base, "0x100000") != 0) {
		string_test_failed("0x100000", cap_info_captured[0].base, "");
		return 1;
	}
	if (strcmp(cap_info_captured[0].top, "0x1311c0") != 0) {
		string_test_failed("0x1311c0", cap_info_captured[0].top, "");
		return 1;
	}
	return 0;
}

int get_all_sym_info_test(sqlite3 *db)
{
	sym_info *sym_info_captured;
	int sym_count = get_all_sym_info(db, &sym_info_captured);
	if (sym_count != 1) {
		numeric_test_failed(1, sym_count, "");
		return 1;
	}
	if (strcmp(sym_info_captured[0].source_path, "/usr/home/psjm3/exercises/print-pointer") != 0) {
		string_test_failed("/usr/home/psjm3/exercises/print-pointer", sym_info_captured[0].source_path, "");
		return 1;
	}
	if (strcmp(sym_info_captured[0].sym_name, "atexit") != 0) {
		string_test_failed("atexit", sym_info_captured[0].sym_name, "");
		return 1;
	}
	if (strcmp(sym_info_captured[0].sym_offset, "0x0") != 0) {
		string_test_failed("0x0", sym_info_captured[0].sym_offset, "");
		return 1;
	}
	if (strcmp(sym_info_captured[0].shndx, "UND") != 0) {
		string_test_failed("UND", sym_info_captured[0].shndx, "");
		return 1;
	}
	if (strcmp(sym_info_captured[0].type, "FUNC") != 0) {
		string_test_failed("FUNC", sym_info_captured[0].type, "");
		return 1;
	}
	if (strcmp(sym_info_captured[0].bind, "GLOBAL") != 0) {
		string_test_failed("GLOBAL", sym_info_captured[0].bind, "");
		return 1;
	}
	if (strcmp(sym_info_captured[0].addr, "0x100000") != 0) {
		string_test_failed("0x100000", sym_info_captured[0].addr, "");
		return 1;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	sqlite3 *db;
	int rc = sqlite3_open(testdb, &db);
	if (rc) {
		char *err_msg;
		asprintf(&err_msg, "Error open test DB %s", sqlite3_errmsg(db));
		numeric_test_failed(SQLITE_OK, rc, err_msg);
		free(err_msg);
		sqlite3_close(db);
		return 1;
	}

	int result = 0;
	
	result = result | prepare_db(db);
	result = result | create_test_tables(db);
	result = result | insert_test_data(db);
	result = result | vm_info_count_test(db);
	result = result | cap_info_count_test(db);
	result = result | sym_info_count_test(db);
	result = result | cap_info_for_lib_count_test(db);
	result = result | get_all_vm_info_test(db);
	result = result | get_all_cap_info_test(db);
	printf("result: %d\n", result);

	remove(testdb);
	return result;
}

/*
\"/usr/home/psjm3/exercises/print-pointer\", \"atexit\", \"0x0\", \"UND\", \"FUNC\", \"GLOBAL\", \"0x100000\");"
*/

