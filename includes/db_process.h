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

#include <sqlite3.h>

#ifndef DB_PROCESS_H_
#define DB_PROCESS_H_

extern char *dbname;

typedef struct vm_info_struct { 
        char *start_addr;       
        char *end_addr; 
        char *mmap_path;
	int compart_id;
	int kve_protection;
        int mmap_flags;         
        int vnode_type;   
  	char *bss_addr;
	char *bss_size;
	char *plt_addr;
	char *plt_size;
	char *got_addr;
	char *got_size;	
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
        char *shndx;    
        char *type;     
        char *bind;
        char *addr; 
} sym_info;

typedef struct comp_info_struct {
    int compart_id;
    char *compart_name;
    char *library_path;
    char *start_addr;
    char *end_addr;
    int is_default;
    int parent_id;
} comp_info;

char *get_dbname(); 
int create_vm_cap_db(sqlite3 *db);
int create_elf_sym_db(sqlite3 *db);
int create_comparts_table(sqlite3 *db);
int sql_query_exec(sqlite3 *db, char* query, int (*callback)(void*,int,char**,char**), void *data); 
int begin_transaction(sqlite3 *db);
int commit_transaction(sqlite3 *db);

int vm_info_count(sqlite3 *db);
int cap_info_count(sqlite3 *db);
int sym_info_count(sqlite3 *db);
int comp_info_count(sqlite3 *db);
int cap_info_for_lib_count(sqlite3 *db, char *lib);

int get_all_vm_info(sqlite3 *db, vm_info **all_vm_info);
int get_all_cap_info(sqlite3 *db, cap_info **all_cap_info);
int get_all_sym_info(sqlite3 *db, sym_info **all_sym_info);
int get_all_comp_info(sqlite3 *db, comp_info **all_comp_info);
int get_cap_info_for_lib(sqlite3 *db, cap_info **cap_info_captured_ptr, char *lib);

#endif //DB_PROCESS_H_
