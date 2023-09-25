#include <sqlite3.h>

#ifndef DB_PROCESS_H_
#define DB_PROCESS_H_

extern char *dbname;

char *get_dbname(); 
int create_vm_cap_db();
int create_elf_sym_db();
int sql_query_exec(char* query, int (*callback)(void*,int,char**,char**), char **data); 
int begin_transaction();
int commit_transaction();

#endif //DB_PROCESS_H_
