/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Jessica Man
 * Copyright (c) 2023 Robert N. M. Watson
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
		char *formatted_sym_info_for_loc = NULL;
		char *formatted_cap_info = NULL;
		char *formatted_sym_info_for_cap = NULL;
		int s;

		xo_open_instance("cap_sym_output");

		/* Capability location information. */
		xo_emit("{:/%12s}", cap_info_captured[i].cap_loc_addr);
		for (s=0; s<sym_count; s++) {
			if (strcmp(cap_info_captured[i].cap_loc_addr, sym_info_captured[s].addr) == 0) {
				asprintf(&formatted_sym_info_for_loc, "%s (%s)",
				    sym_info_captured[s].sym_name,
				    sym_info_captured[s].type);
				break;
			}
		}
		if (s == sym_count)
			asprintf(&formatted_sym_info_for_loc, "%s (%s)", "-", "-");
		xo_emit("{:/  %43-s}", formatted_sym_info_for_loc);
		free(formatted_sym_info_for_loc);

		/* Capability range and permissions. */
		asprintf(&formatted_cap_info, "%s[%s,-%s]",
		    cap_info_captured[i].cap_addr,
		    cap_info_captured[i].perms,
		    cap_info_captured[i].top);
		xo_emit("{:capinfo/% 45-s}", formatted_cap_info);
		free(formatted_cap_info);

		/* Capability target information. */
		for (s=0; s<sym_count; s++) {
			if (strcmp(cap_info_captured[i].cap_addr, sym_info_captured[s].addr) == 0) {
				asprintf(&formatted_sym_info_for_cap, "%s (%s)",
				    sym_info_captured[s].sym_name,
				    sym_info_captured[s].type);
				break;
			}
		}
		if (s == sym_count)
			asprintf(&formatted_sym_info_for_cap, "%s (%s)",
			    "-", "-");
		xo_emit("{:/ %43-s}\n", formatted_sym_info_for_cap);
		free(formatted_sym_info_for_cap);

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

