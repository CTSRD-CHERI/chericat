#include <sqlite3.h>

#ifndef DB_PROCESS_H_
#define DB_PROCESS_H_

extern char *dbname;

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
        char *shndx;    
        char *type;     
        char *bind;
        char *addr; 
} sym_info;

char *get_dbname(); 
int create_vm_cap_db(sqlite3 *db);
int create_elf_sym_db(sqlite3 *db);
int sql_query_exec(sqlite3 *db, char* query, int (*callback)(void*,int,char**,char**), void *data); 
int begin_transaction(sqlite3 *db);
int commit_transaction(sqlite3 *db);

int vm_info_count(sqlite3 *db);
int cap_info_count(sqlite3 *db);
int sym_info_count(sqlite3 *db);
int cap_info_for_lib_count(sqlite3 *db, char *lib);

int get_all_vm_info(sqlite3 *db, vm_info **all_vm_info);
int get_all_cap_info(sqlite3 *db, cap_info **all_cap_info);
int get_all_sym_info(sqlite3 *db, sym_info **all_sym_info);
int get_cap_info_for_lib(sqlite3 *db, cap_info **cap_info_captured_ptr, char *lib);

#endif //DB_PROCESS_H_
