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
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <libelf.h>
#include <gelf.h>
#include <err.h>
#include <link.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/user.h>
#include <sys/wait.h>
#include <sys/time.h>

#include "common.h"
#include "db_process.h"
#include "elf_utils.h"

static const char *
st_bind(unsigned int sbind)
{
	static char s_sbind[32];

	switch (sbind) {
	case STB_LOCAL: return "LOCAL";
	case STB_GLOBAL: return "GLOBAL";
	case STB_WEAK: return "WEAK";
	case STB_GNU_UNIQUE: return "UNIQUE";
	default:
		if (sbind >= STB_LOOS && sbind <= STB_HIOS)
			return "OS";
		else if (sbind >= STB_LOPROC && sbind <= STB_HIPROC)
			return "PROC";
		else
			snprintf(s_sbind, sizeof(s_sbind), "<unknown: %#x>",
			    sbind);
		return (s_sbind);
	}
}

static const char *
st_type(unsigned int mach, unsigned int os, unsigned int stype)
{
	static char s_stype[32];

	switch (stype) {
	case STT_NOTYPE: return "NOTYPE";
	case STT_OBJECT: return "OBJECT";
	case STT_FUNC: return "FUNC";
	case STT_SECTION: return "SECTION";
	case STT_FILE: return "FILE";
	case STT_COMMON: return "COMMON";
	case STT_TLS: return "TLS";
	default:
		if (stype >= STT_LOOS && stype <= STT_HIOS) {
			if ((os == ELFOSABI_GNU || os == ELFOSABI_FREEBSD) &&
			    stype == STT_GNU_IFUNC)
				return "IFUNC";
			snprintf(s_stype, sizeof(s_stype), "OS+%#x",
			    stype - STT_LOOS);
		} else if (stype >= STT_LOPROC && stype <= STT_HIPROC) {
			if (mach == EM_SPARCV9 && stype == STT_SPARC_REGISTER)
				return "REGISTER";
			snprintf(s_stype, sizeof(s_stype), "PROC+%#x",
			    stype - STT_LOPROC);
		} else
			snprintf(s_stype, sizeof(s_stype), "<unknown: %#x>",
			    stype);
		return (s_stype);
	}
}

static const char *
st_shndx(unsigned int shndx)
{
	static char s_shndx[32];

	switch (shndx) {
	case SHN_UNDEF: return "UND";
	case SHN_ABS: return "ABS";
	case SHN_COMMON: return "COM";
	default:
		if (shndx >= SHN_LOPROC && shndx <= SHN_HIPROC)
			return "PRC";
		else if (shndx >= SHN_LOOS && shndx <= SHN_HIOS)
			return "OS";
		else
			snprintf(s_shndx, sizeof(s_shndx), "%u", shndx);
		return (s_shndx);
	}
}

Elf *read_elf(char *path) {
	if (elf_version(EV_CURRENT) == EV_NONE) {
		fprintf(stderr, "ELF library initialisation failed %s\n", elf_errmsg(-1));
		exit(1);
	}
		
	int fd;
	if ((fd = open(path, O_RDONLY, 0)) < 0) {
		fprintf(stderr, "Error getting the ELF file from %s\n", path);
		exit(1);
	}
			
	Elf *elfFile;

	if ((elfFile = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		fprintf(stderr, "Error reading ELF file %s\n", elf_errmsg(-1));
		exit(1);
	}
				
	return elfFile;
}

void get_elf_info(sqlite3 *db, Elf *elfFile, char *source, u_long source_base, special_sections **ssect, int ssect_index) 
{
	debug_print(TROUBLESHOOT, "Key Stage: Create database and tables to store symbols obtained from ELF of the loaded binaries\n", NULL);
	create_elf_sym_db(db);

	GElf_Ehdr ehdr;
	GElf_Shdr shdr;
	GElf_Sym sym;
	Elf_Data *data;
	Elf_Scn *scn;
	const char *symname;
	Elf_Word strscnidx;
	size_t shstrndx;

	if (gelf_getehdr(elfFile, &ehdr) == NULL) {
		fprintf(stderr, "Can't read ELF header for %s\n", source);
		elf_end(elfFile);
	}

	if (elf_getshdrstrndx(elfFile, &shstrndx) != 0) {
		fprintf(stderr, "elf_getshdrstrndx() failed: %s\n", elf_errmsg(-1));
	}

	scn = NULL;
	bool seen_dynsym=0, seen_symtab=0;

	char *insert_syms_query_values;
	int query_values_index=0;
	//u_long bss_addr, plt_addr, got_addr;
	//size_t bss_size, plt_size, got_size;

	special_sections **ssect_cast = (special_sections **)ssect;

	while ((scn = elf_nextscn(elfFile, scn)) != NULL) {
		gelf_getshdr(scn, &shdr);

		char *section_name;
		if ((section_name = elf_strptr(elfFile, shstrndx, shdr.sh_name)) == NULL) {
			fprintf(stderr, "elf_strptr() failed: %s\n", elf_errmsg(-1));
		}

		special_sections ssect_captured;
		if (strcmp(section_name, ".bss") == 0) {
			ssect_captured.bss_addr = shdr.sh_addr+source_base;
			ssect_captured.bss_size = shdr.sh_size;

		}
                if (strcmp(section_name, ".plt") == 0) {
                        ssect_captured.plt_addr = shdr.sh_addr+source_base;
			ssect_captured.plt_size = shdr.sh_size;
                }
                if (strcmp(section_name, ".got") == 0) {
                        ssect_captured.got_addr = shdr.sh_addr+source_base;
			ssect_captured.got_size = shdr.sh_size;
                }

		ssect_captured.mmap_path = strdup(source);
		(*ssect_cast)[ssect_index] = ssect_captured;

		if (shdr.sh_type == SHT_DYNSYM || shdr.sh_type == SHT_SYMTAB) {
			if (shdr.sh_type == SHT_DYNSYM) {
				seen_dynsym = 1;
			}
			if (shdr.sh_type == SHT_SYMTAB) {
				seen_symtab = 1;
			}
			assert(scn != NULL);		
			strscnidx = shdr.sh_link;
			data = elf_getdata(scn, NULL);
			assert(data != NULL);

			for (int i=0; gelf_getsym(data, i, &sym) != NULL; i++) {
				symname = elf_strptr(elfFile, strscnidx, sym.st_name);
				if (symname == NULL) {
					continue;
				} else {
					// Calculate and store the address of the symbol using the base and offset
					u_long offset = sym.st_value;

					char* query_value;
					asprintf(&query_value, 
						"(\"%s\", \"%s\", \"0x%lx\", \"%3s\", \"%s\", \"%s\", \"0x%lx\")", 
							source,
							symname,
							sym.st_value,
							st_shndx(sym.st_shndx),
							st_type(ehdr.e_machine, ehdr.e_ident[EI_OSABI], GELF_ST_TYPE(sym.st_info)),
							st_bind(GELF_ST_BIND(sym.st_info)),
							(source_base+offset));
					if (query_values_index == 0) {
						insert_syms_query_values = strdup(query_value);
					} else {
						char *temp;
						asprintf(&temp, "%s,%s",
							insert_syms_query_values,
							query_value);
						insert_syms_query_values = strdup(temp);
						free(temp);
					}
					free(query_value);
					query_values_index++;
					
					if ((i % 200) == 0) {
						if (insert_syms_query_values != NULL) {
							char query_hdr[] = "INSERT INTO elf_sym VALUES ";
							char* query;
							asprintf(&query, "%s%s;", query_hdr, insert_syms_query_values);

							int db_rc = sql_query_exec(db, query, NULL, NULL);
							debug_print(TROUBLESHOOT, "Key Stage: Inserted sym info to the database (rc=%d)\n", db_rc);

							insert_syms_query_values = NULL;
							query_values_index = 0;
							free(query);
						}
					}

				}
			}	
		}
		if (seen_dynsym && seen_symtab) {
			break;
		}
	}
	
	if (insert_syms_query_values != NULL) {
		char query_hdr[] = "INSERT INTO elf_sym VALUES ";
		char* query;
		asprintf(&query, "%s%s;", query_hdr, insert_syms_query_values);

		int db_rc = sql_query_exec(db, query, NULL, NULL);
		debug_print(TROUBLESHOOT, "Key Stage: Inserted sym info to the database (rc=%d)\n", db_rc);

		free(insert_syms_query_values);
		free(query);
	}
	elf_end(elfFile);
	
}

