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

#include "common.h"
#include "db_process.h"

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
	char *kind;
	Elf_Kind elfKind;

	if ((elfFile = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		fprintf(stderr, "Error reading ELF file %s\n", elf_errmsg(-1));
		exit(1);
	}
	elfKind = elf_kind(elfFile);

	switch (elfKind) {
		case ELF_K_AR:
			kind = "ar(1) archive";
			break;
		case ELF_K_ELF:
			kind = "elf object";
			break;
		case ELF_K_NONE:
			kind = "data";
			break;
		default:
			kind = "unrecognized";
	}
				
	return elfFile;
}

void get_elf_info(sqlite3 *db, Elf *elfFile, char *source, u_long source_base) 
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

	if (gelf_getehdr(elfFile, &ehdr) == NULL) {
		fprintf(stderr, "Can't read ELF header for %s\n", source);
		elf_end(elfFile);
	}

	scn = NULL;
	bool seen_dynsym=0, seen_symtab=0;

	char *insert_syms_query_values;
	int query_values_index=0;

	while ((scn = elf_nextscn(elfFile, scn)) != NULL) {
		gelf_getshdr(scn, &shdr);
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

					//debug_print(TROUBLESHOOT, "Key Stage: Inserted elf symbol entry info to the database (rc=%d)\n", db_rc);
				}
			}	
		}
		if (seen_dynsym && seen_symtab) {
			break;
		}
	}
	
	if (insert_syms_query_values != NULL) {
		char query_hdr[] = "INSERT INTO elf_sym VALUES";
		char* query;
		asprintf(&query, "%s%s;", query_hdr, insert_syms_query_values);

		int db_rc = sql_query_exec(db, query, NULL, NULL);
		debug_print(TROUBLESHOOT, "Key Stage: Inserted sym info to the database (rc=%d)\n", db_rc);
		free(insert_syms_query_values);
		free(query);
	}

	elf_end(elfFile);
	
}

