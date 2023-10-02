#include <libelf.h>

#ifndef ELF_UTILS_H_
#define ELF_UTILS_H_

typedef struct special_sections_struct {
	u_long bss_addr;
	size_t bss_size;
	u_long plt_addr;
	size_t plt_size;
	u_long got_addr;
	size_t got_size;
	char *mmap_path;
} special_sections;

Elf *read_elf(char *path);
void get_elf_info(sqlite3 *db, Elf *elfFile, char *source, u_long source_base, special_sections **ssect, int ssect_index);

#endif //ELF_UTILS_H_
