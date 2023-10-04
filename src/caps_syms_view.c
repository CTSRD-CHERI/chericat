#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <libxo/xo.h>

#include "db_process.h"

/*
 * cap_sym_view
 * SQLite callback routine to traverse results from sqlite_exec()
 * using:
 * 
 * 1. "SELECT * FROM cap_info;"
 *
 * 2. "SELECT COuNT(*) FROM elf_sym;"
 *
 * 3. "SELECT * FROM elf_sym;"
 * 
 */
void caps_syms_view(sqlite3 *db, char *lib) 
{
	cap_info *cap_info_captured;
	//int cap_count = get_all_cap_info(db, &cap_info_captured);
	int cap_count = get_cap_info_for_lib(db, &cap_info_captured, lib);

	sym_info *sym_info_captured;
	int sym_count = get_all_sym_info(db, &sym_info_captured);

	xo_open_list("cap_sym_output");

	int dbname_len = strlen(get_dbname());
	for (int l=0; l<dbname_len+30; l++) {	
		xo_emit("{:top_dash/-}");
	}
	xo_emit("{:total_caps/\n%s - total number of caps: %d\n}", get_dbname(), cap_count);
	for (int l=0; l<dbname_len+30; l++) {	
		xo_emit("{:bottom_dash/-}");
	}

	xo_emit("{T:/\n%12s %43-s %45-s %s}\n",
		"CAP_LOC", " CAP_LOC_SYM (TYPE)", "CAP_INFO", "CAP_SYM (TYPE)");
		
	for (int i=0; i<cap_count; i++) {
		xo_open_instance("cap_sym_output");

		char *sym_name_for_cap_loc[50];
		char *type_for_cap_loc[50];
		int sym_name_for_cap_loc_index=0, type_for_cap_loc_index=0;

		char *sym_name_for_cap[50];
		char *type_for_cap[50];
		int sym_name_for_cap_index=0, type_for_cap_index=0;

		for (int s=0; s<sym_count; s++) {
			if (strcmp(cap_info_captured[i].cap_loc_addr, sym_info_captured[s].addr) == 0) {
				sym_name_for_cap_loc[sym_name_for_cap_loc_index++] = strdup(sym_info_captured[s].sym_name);
				type_for_cap_loc[type_for_cap_loc_index++] = strdup(sym_info_captured[s].type);
			}
			
			if (strcmp(cap_info_captured[i].cap_addr, sym_info_captured[s].addr) == 0) {
				sym_name_for_cap[sym_name_for_cap_index++] = strdup(sym_info_captured[s].sym_name);
				type_for_cap[type_for_cap_index++] = strdup(sym_info_captured[s].type);
			}
		}

		for (int n=0; n<sym_name_for_cap_loc_index; n++) {
			xo_emit("{:/%12s}", cap_info_captured[i].cap_loc_addr);

			char *formatted_sym_info_for_loc = NULL;
			asprintf(&formatted_sym_info_for_loc, "%s (%s)",
					sym_name_for_cap_loc[n],
					type_for_cap_loc[n]);
			
			xo_emit("{:/  %43-s}", formatted_sym_info_for_loc);
			free(formatted_sym_info_for_loc);

			char *formatted_cap_info = NULL;
			asprintf(&formatted_cap_info, "%s[%s,-%s]",
				cap_info_captured[i].cap_addr,
				cap_info_captured[i].perms,
				cap_info_captured[i].base,
				cap_info_captured[i].top);
			xo_emit("{:capinfo/% 45-s}", formatted_cap_info);
			free(formatted_cap_info);

			if (sym_name_for_cap_index > 0) {
				char *formatted_sym_info_for_cap = NULL;
				asprintf(&formatted_sym_info_for_cap, "%s (%s)",
						sym_name_for_cap[0],
						type_for_cap[0]);
				xo_emit("{:/ %43-s}\n", formatted_sym_info_for_cap);
				free(formatted_sym_info_for_cap);
			} else {
				xo_emit("{:/ %40-s}\n", "SYM NOT FOUND");
			}
		}

		xo_close_instance("cap_sym_output");

		free(cap_info_captured[i].cap_loc_addr);
		free(cap_info_captured[i].cap_loc_path);
		free(cap_info_captured[i].cap_addr);
		free(cap_info_captured[i].perms);
		free(cap_info_captured[i].base);
		free(cap_info_captured[i].top);
	}

	for (int k=0; k<sym_count; k++) {
		free(sym_info_captured[k].source_path);
		free(sym_info_captured[k].sym_name);
		free(sym_info_captured[k].sym_offset);
		free(sym_info_captured[k].shndx);
		free(sym_info_captured[k].type);
		free(sym_info_captured[k].bind);
		free(sym_info_captured[k].addr);
	}

	xo_close_list("cap_sym_output");
	free(cap_info_captured);
	free(sym_info_captured);
}

