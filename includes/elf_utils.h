#include <libelf.h>

#ifndef ELF_UTILS_H_
#define ELF_UTILS_H_

Elf *read_elf(char *path);
void get_elf_info(Elf *elfFile, char *source);

#endif //ELF_UTILS_H_
