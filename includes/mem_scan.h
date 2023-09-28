#ifndef MEM_SCAN_H_
#define MEM_SCAN_H_

#include <sqlite3.h>

void scan_mem(sqlite3 *db, char* arg_pid);
void scan_mem_using_procstat(sqlite3 *db, char* arg_pid);

#endif //MEM_SCAN_H_
