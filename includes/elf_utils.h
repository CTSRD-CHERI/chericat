#include <libelf.h>

#ifndef ELF_UTILS_H_
#define ELF_UTILS_H_

Elf *read_elf(char *path);
void get_elf_info(sqlite3 *db, Elf *elfFile, char *source, u_long source_base);

#endif //ELF_UTILS_H_
