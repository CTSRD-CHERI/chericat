#include <sqlite3.h>

#ifndef DB_PROCESS_H_
#define DB_PROCESS_H_

extern char *dbname;

char *get_dbname(); 
int create_vm_cap_db(sqlite3 *db);
int create_elf_sym_db(sqlite3 *db);
int sql_query_exec(sqlite3 *db, char* query, int (*callback)(void*,int,char**,char**), char **data); 
int begin_transaction(sqlite3 *db);
int commit_transaction(sqlite3 *db);

#endif //DB_PROCESS_H_
