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

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>

#include <libxo/xo.h>
#include <cheri/cheric.h>

#include "common.h"
#include "mem_scan.h"
#include "db_process.h"
#include "ptrace_utils.h"
#include "vm_caps_view.h"
#include "caps_syms_view.h"

/*
 * usage()
 * Useful message to users to show how to run the program and what parameters are accepted.
 */

static void usage()
{
	fprintf(stderr, "Usage: chericat [-d] [-f <database name>] [-p <pid>] [-v]\n\t[-c <binary name>]\n"
			"     pid           - pid of the target process for a snapshot of caps info\n"
			"     database name - name of the database to store data captured by chericat\n"
			"     binary name   - name of the binary file for which to show the\n"
			"                     capabilities located, with corresponding symbols\n"
			"Options:\n"
			"     -d Enable debugging output. Repeated -d's (up to 3) increase verbosity.\n" 
			"     -f Provide the database name to capture the data collected by chericat.\n"
			"        If omitted, an in-memory db is used\n"
			"     -p Scan the mapped memory and persist the caps data to a database\n"
			"     -v Show virtual summary info of capabilities in the target process,\n"
			"        arranged in mmap order\n"
			"     -c Show capabilities with corresponding symbols located in the provided\n"
			"        binary\n");
}

static struct option long_options[] = 
{
	{"debug_level", no_argument, 0, 'd'},
	{"database_name", required_argument, 0, 'f'},
	{"scan_mem", required_argument, 0, 'p'},
	{"caps_summary", no_argument, 0, 'v'},
	{"caps_symbols_summary", required_argument, 0, 'c'},
	{0,0,0,0}
};

int 
main(int argc, char *argv[])
{
	// libxo API to parse the libxo command line arguments. They are removed once parsed and stored, 
	// the program arguments would then be handled as intended without the libxo arguments.
	argc = xo_parse_args(argc, argv);

	int optindex;
	int opt = getopt_long(argc, argv, "df:p:vc:", long_options, &optindex);

	if (opt == -1) {
		usage();
		return 0;
	}

	sqlite3 *db = NULL;

	static int debug_level = 0;
	set_print_level(debug_level);

	while (opt != -1) {
		switch (opt)
		{
		case 'p':
			if (db == NULL) {
        			int rc = sqlite3_open(get_dbname(), &db);

        			if (rc) {
                			fprintf(stderr, "Error open DB %s", sqlite3_errmsg(db));
              				sqlite3_close(db);
               		 		return (1);
        			}
			}

			char *pEnd;
			long int pid = strtol(optarg, &pEnd, 10);

			if (*pEnd != '\0') {
				errx(1, "%s is not a valid pid", optarg);
			}

			scan_mem(db, pid);
			break;
		case 'v':
			if (db == NULL) {
        			int rc = sqlite3_open(get_dbname(), &db);

        			if (rc) {
                			fprintf(stderr, "Error open DB %s", sqlite3_errmsg(db));
              				sqlite3_close(db);
                			return (1);
        			}
			}
			xo_open_list("vm_cap_output");
			vm_caps_view(db);
			xo_close_list("vm_cap_output");
			break;
		case 'c':
			if (db == NULL) {
        			int rc = sqlite3_open(get_dbname(), &db);
	
        			if (rc) {
                			fprintf(stderr, "Error open DB %s", sqlite3_errmsg(db));
              				sqlite3_close(db);
                			return (1);
        			}
			}
			caps_syms_view(db, optarg);
			break;
		case 'f':
			dbname = (char*)malloc(strlen(optarg)+1);
			strcpy(dbname, optarg);
			break;
		case 'd':
			debug_level++;
			set_print_level(debug_level);
			break;
		default:
			usage();
			exit(0);
		}
		opt = getopt_long(argc, argv, "df:p:vc:", long_options, &optindex);
	}

	if (db != NULL) {
		sqlite3_close(db);
	}
        return 0;
}

