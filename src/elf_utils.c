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
#include <dwarf.h>
#include <libdwarf.h>
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
		return;
	}

	if (elf_getshdrstrndx(elfFile, &shstrndx) != 0) {
		fprintf(stderr, "elf_getshdrstrndx() failed: %s\n", elf_errmsg(-1));
		elf_end(elfFile);
		return;
	}

	scn = NULL;
	bool seen_dynsym=0, seen_symtab=0;

	char *insert_syms_query_values;
	int query_values_index=0;

	special_sections **ssect_cast = (special_sections **)ssect;

	while ((scn = elf_nextscn(elfFile, scn)) != NULL) {
		gelf_getshdr(scn, &shdr);

		char *section_name;
		if ((section_name = elf_strptr(elfFile, shstrndx, shdr.sh_name)) == NULL) {
			fprintf(stderr, "elf_strptr() failed: %s\n", elf_errmsg(-1));
		}

		special_sections ssect_captured;
		/*
		if (strcmp(section_name, ".bss") == 0) {
			ssect_captured.bss_addr = shdr.sh_addr+source_base;
			ssect_captured.bss_size = shdr.sh_size;

		}
		*/
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

void dump_dwarf_die(Dwarf_Debug dbg, Dwarf_Die die, int level)
{
	Dwarf_Attribute *attr_list;
	Dwarf_Die ret_die;
	Dwarf_Off dieoff, cuoff, culen, attroff;
	Dwarf_Unsigned ate, lang, v_udata, v_sig;
	Dwarf_Signed attr_count, v_sdata;
	Dwarf_Off v_off;
	Dwarf_Addr v_addr;
	Dwarf_Half tag, attr, form;
	Dwarf_Block *v_block;
	Dwarf_Bool v_bool, is_info;
	Dwarf_Sig8 v_sig8;
	Dwarf_Error de;
	Dwarf_Ptr v_expr;
	const char *tag_str, *attr_str, *ate_str, *lang_str;
	char unk_tag[32], unk_attr[32];
	char *v_str;
	uint8_t *b, *p;
	int i, j, abc, ret;

	if (dwarf_dieoffset(die, &dieoff, &de) != DW_DLV_OK) {
		warnx("dwarf_dieoffset failed: %s", dwarf_errmsg(de));
		goto cont_search;
	}

	//printf(" <%d><%jx>: ", level, (uintmax_t) dieoff);

	if (dwarf_die_CU_offset_range(die, &cuoff, &culen, &de) != DW_DLV_OK) {
		warnx("dwarf_die_CU_offset_range failed: %s",
		      dwarf_errmsg(de));
		cuoff = 0;
	}

	abc = dwarf_die_abbrev_code(die);
	if (dwarf_tag(die, &tag, &de) != DW_DLV_OK) {
		warnx("dwarf_tag failed: %s", dwarf_errmsg(de));
		goto cont_search;
	}
	if (dwarf_get_TAG_name(tag, &tag_str) != DW_DLV_OK) {
		snprintf(unk_tag, sizeof(unk_tag), "[Unknown Tag: %#x]", tag);
		tag_str = unk_tag;
	}
	
	if (strcmp(tag_str, "DW_TAG_variable") == 0) {

		//printf("Abbrev Number: %d (%s)\n", abc, tag_str);
	
		if ((ret = dwarf_attrlist(die, &attr_list, &attr_count, &de)) != DW_DLV_OK) {
			if (ret == DW_DLV_ERROR)
				warnx("dwarf_attrlist failed: %s", dwarf_errmsg(de));
			goto cont_search;
		}

		for (i = 0; i < attr_count; i++) {
			if (dwarf_whatform(attr_list[i], &form, &de) != DW_DLV_OK) {
				warnx("dwarf_whatform failed: %s", dwarf_errmsg(de));
				continue;
			}
			if (dwarf_whatattr(attr_list[i], &attr, &de) != DW_DLV_OK) {
				warnx("dwarf_whatattr failed: %s", dwarf_errmsg(de));
				continue;
			}
			if (dwarf_get_AT_name(attr, &attr_str) != DW_DLV_OK) {
				snprintf(unk_attr, sizeof(unk_attr), "[Unknown AT: %#x]", attr);
				attr_str = unk_attr;
			}
			if (dwarf_attroffset(attr_list[i], &attroff, &de) != DW_DLV_OK) {
				warnx("dwarf_attroffset failed: %s", dwarf_errmsg(de));
				attroff = 0;
			}
			//printf("    <%jx>   %-18s: ", (uintmax_t) attroff, attr_str);
			
			switch (form) {
			case DW_FORM_string:
			case DW_FORM_strp:
				if (dwarf_formstring(attr_list[i], &v_str, &de) != DW_DLV_OK) {
					warnx("dwarf_formstring failed: %s", dwarf_errmsg(de));
					continue;
				}
				if (form == DW_FORM_string)
					printf("%s\n", v_str);
				else
					printf("(indirect string) %s\n", v_str);
				break;

			default:
				break;
			}
			//putchar('\n');
		}
	}


cont_search:
	/* Search children. */
	ret = dwarf_child(die, &ret_die, &de);
	if (ret == DW_DLV_ERROR)
		warnx("dwarf_child: %s", dwarf_errmsg(de));
	else if (ret == DW_DLV_OK)
		dump_dwarf_die(dbg, ret_die, level + 1);

	/* Search sibling. */
	is_info = dwarf_get_die_infotypes_flag(die);
	ret = dwarf_siblingof_b(dbg, die, &ret_die, is_info, &de);
	if (ret == DW_DLV_ERROR)
		warnx("dwarf_siblingof: %s", dwarf_errmsg(de));
	else if (ret == DW_DLV_OK)
		dump_dwarf_die(dbg, ret_die, level);

	dwarf_dealloc(dbg, die, DW_DLA_DIE);
}

void dump_dwarf_cu(Dwarf_Debug dbg) 
{
	int ret;
	Dwarf_Half tag, version, pointer_size, off_size;
	Dwarf_Off cu_offset, cu_length;
	Dwarf_Off aboff;
	Dwarf_Sig8 sig8;
	Dwarf_Error de;
	Dwarf_Die die;
	Dwarf_Unsigned typeoff;

	Dwarf_Bool is_info = 1;

	do {
		printf("\nDump of debug contents of section\n");

		while ((ret = dwarf_next_cu_header_c(dbg, is_info, NULL,
		    &version, &aboff, &pointer_size, &off_size, NULL, &sig8,
		    &typeoff, NULL, &de)) == DW_DLV_OK) {
			die = NULL;
			while (dwarf_siblingof_b(dbg, die, &die, is_info, &de) == DW_DLV_OK) {
				if (dwarf_tag(die, &tag, &de) != DW_DLV_OK) {
					warnx("dwarf_tag failed: %s",
					    dwarf_errmsg(de));
					continue;
				}
				/* XXX: What about DW_TAG_partial_unit? */
				if ((is_info && tag == DW_TAG_compile_unit) ||
				    (!is_info && tag == DW_TAG_type_unit))
					break;
			}
			if (die == NULL && is_info) {
				warnx("could not find DW_TAG_compile_unit "
				    "die");
				continue;
			} else if (die == NULL && !is_info) {
				warnx("could not find DW_TAG_type_unit die");
				continue;
			}

			if (dwarf_die_CU_offset_range(die, &cu_offset, &cu_length, &de) != DW_DLV_OK) {
				warnx("dwarf_die_CU_offset failed: %s",
				    dwarf_errmsg(de));
				continue;
			}

			cu_length -= off_size == 4 ? 4 : 12;

			dump_dwarf_die(dbg, die, 0);
		}
		printf("ret: %d\n", ret);

		if (ret == DW_DLV_ERROR)
			warnx("dwarf_next_cu_header: %s", dwarf_errmsg(de));
		if (is_info)
			break;
	} while (dwarf_next_types_section(dbg, &de) == DW_DLV_OK);
}

Elf *read_debug_symbols(char *debug_file) {
	char *true_path = 0;
	unsigned int true_pathlen = 0;
	Dwarf_Handler errhand = 0;
	Dwarf_Ptr errarg = 0;
	Dwarf_Error error;
	int res = 0;
	Dwarf_Debug dbg = 0;

	if (elf_version(EV_CURRENT) == EV_NONE) {
		fprintf(stderr, "ELF library initialisation failed %s\n", elf_errmsg(-1));
		exit(1);
	}
		
	int fd;
	if ((fd = open(debug_file, O_RDONLY, 0)) < 0) {
		fprintf(stderr, "Error getting the ELF file from %s\n", debug_file);
		exit(1);
	}
			
	Elf *elfFile;

	if ((elfFile = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		fprintf(stderr, "Error reading ELF file %s\n", elf_errmsg(-1));
		exit(1);
	}

	res = dwarf_elf_init(elfFile, DW_DLC_READ, NULL, NULL, &dbg, &error);
	printf("OK: %d\n", res);

	dump_dwarf_cu(dbg);
	
	return elfFile;
}


