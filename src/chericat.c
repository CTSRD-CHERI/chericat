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
#include <signal.h>

#include <libxo/xo.h>
#include <cheri/cheric.h>

#include "common.h"
#include "mem_scan.h"
#include "db_process.h"
#include "ptrace_utils.h"
#include "vm_caps_view.h"
#include "caps_syms_view.h"
#include "rtld_linkmap_scan.h"

/* Used for signal handling, if a process has been
 * attached to Chericat, we should do a ptrace_detach 
 * and close the sqlite db before exiting when a 
 * terminating signal is received for Chericat
 */
int target_pid=-1;
sqlite3 *db = NULL;

/*
 * usage()
 * Useful message to users to show how to run the program and what parameters are accepted.
 */

static void usage()
{
	fprintf(stderr, "Usage: chericat [-d] [-f <database name>] [-p <pid>] [-v]\n\t[-l <pid>] [-c <binary name>]\n"
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
			"     -l Scan RTLD linkmap for compartmentalisation information in the target process\n"
			"     -c Show capabilities with corresponding symbols located in the provided\n"
			"        binary\n");
}

static struct option long_options[] = 
{
	{"debug_level", no_argument, 0, 'd'},
	{"database_name", required_argument, 0, 'f'},
	{"scan_mem", required_argument, 0, 'p'},
	{"caps_summary", no_argument, 0, 'v'},
	{"rtld_linkmap_scan", required_argument, 0, 'l'},
	{"caps_symbols_summary", required_argument, 0, 'c'},
	{0,0,0,0}
};

void handle_terminate(int sig)
{
	if (target_pid != -1) {
		ptrace_detach(target_pid);
	}

	xo_finish();
	if (db != NULL) {
		sqlite3_close(db);
	}
	exit(sig);
}


int 
main(int argc, char *argv[])
{
	// libxo API to parse the libxo command line arguments. They are removed once parsed and stored, 
	// the program arguments would then be handled as intended without the libxo arguments.
	argc = xo_parse_args(argc, argv);

	int optindex;
	int opt = getopt_long(argc, argv, "df:p:vl:c:", long_options, &optindex);

	if (opt == -1) {
		usage();
		return 0;
	}

	static int debug_level = 0;
	set_print_level(debug_level);

#ifdef HAVE_SIGACTION
	{
	struct sigaction sa;
	sa.sa_flags=0;
	sa.sa_handler = sigterm;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGPIPE, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
#else
	signal(SIGTERM, handle_terminate);
	signal(SIGINT, handle_terminate);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGQUIT, handle_terminate);
#endif

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

			target_pid = pid;
			
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
			xo_open_container("vm_view");
			vm_caps_view(db);
			xo_close_container("vm_view");
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
			xo_open_container("cap_view");
			caps_syms_view(db, optarg);
			xo_close_container("cap_view");
			break;
		case 'l':
			getprocs_with_procstat_sysctl(db, optarg);
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
		opt = getopt_long(argc, argv, "df:p:vl:c:", long_options, &optindex);
	}

	xo_finish();

	if (db != NULL) {
		sqlite3_close(db);
	}
        return 0;
}

