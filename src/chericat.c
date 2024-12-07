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

#include <err.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <libxo/xo.h>

#include "common.h"
#include "db_process.h"

#include "caps_syms_view.h"
#include "mem_scan.h"
#include "ptrace_utils.h"
#include "rtld_linkmap_scan.h"
#include "vm_caps_view.h"

sqlite3 *db = NULL;

static int exit_usage(const char *msg)
{
    if (msg) {
        fprintf(stderr, "Bad arguments: %s\n", msg);
    }
    fprintf(stderr, "Usage: chericat\n\t"
            "[-d|--debug]\n\t"
            "[-f|--db <database name>]\n\t"
            "[-p|--attach <pid>]\n\t"
            "[[-l|--lib -v|--overview] | [-c|--compart -v|--overview]]\n\t"
            "[[-l|--lib -i|--caps_info <library name>] | [-c|--compart -i|--caps_info <compartment name>]]\n"
            "    database name    - name of the database to store data captured by chericat\n"
            "    pid              - pid of the target process\n"
            "    library name     - name of the library for which show the capabilities info\n"
            "    compartment name - name of the compartment for which show the capabilities info\n"
            "Options:\n"
            "    -d Enable debugging output. Repeated -d's (up to 3) increase verbosity.\n"
            "    -f Provide the database name to capture the data collected.\n"
            "       If omitted an in-memory db is used\n"
            "    -p Scan the vm blocks and persist the data to the provided database.\n"
            "    -l Show the capabilities data in library-centric view.\n"
            "    -c Show the capabilities data in compartment-centric view.\n"
            "    -v Show the vm info, arranged in either library- or compartment-centric view\n"
            "    -i Show capabalities found in the provided library or compartment\n");
    exit(1);
}

static struct option long_options[] =
{
    {"debug", no_argument, 0, 'd'},
    {"db", required_argument, 0, 'f'},
    {"attach", required_argument, 0, 'p'},
    {"lib", no_argument, 0, 'l'},
    {"compart", no_argument, 0, 'c'},
    {"overview", no_argument, 0, 'v'},
    {"caps_info", required_argument, 0, 'i'},
    {0,0,0,0}
};

void terminate_chericat(int sig)
{
    xo_finish();
    if (db != NULL) {
	sqlite3_close(db);
    }
    exit(sig);
}

int main(int argc, char **argv)
{
    // libxo API to parse the libxo command line arguments. They are removed once parsed and stored,
    // the program arguments would then be handled as intended without the libxo arguments.
    argc = xo_parse_args(argc, argv);
   
    int opt_f=0;
    int opt_p=0;
    int opt_l=0;
    int opt_c=0;
    int opt_v=0;
    int opt_i=0;
 
    long int pid=-1;
    char *pEnd;
    char *caps_info_param;
    
    int optindex;
    int opt = getopt_long(argc, argv, "df:p:lcvi:", long_options, &optindex);
    
    if (opt == -1) {
        exit_usage(NULL);
    }

    static int debug_level = 0;
    set_print_level(debug_level);

    signal(SIGTERM, terminate_chericat);
    signal(SIGINT, terminate_chericat);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGQUIT, terminate_chericat);
    
    while (opt != -1) {
        switch(opt) {
	    case 'd':
		debug_level++;
		set_print_level(debug_level);
		break;
	    case 'f':
		dbname = optarg;
		++opt_f;
		break;
	    case 'p':
		pid = strtol(optarg, &pEnd, 10);
		if (*pEnd != '\0') {
		    errx(1, "%s is not a valid pid", optarg);
		}
		++opt_p;
		break;
            case 'l':
                if (opt_c != 0) {
		    exit_usage("Either -l (library view) or -c (compartment view) is allowed");
		}
                ++opt_l;
                break;
    	    case 'c':
                if (opt_l != 0) {
		    exit_usage("Either -l (library view) or -c (compartment view) is allowed");
		}
                ++opt_c;
                break;
            case 'v':
                ++opt_v;
                break;
	    case 'i':
		caps_info_param = optarg;
		++opt_i;
		break;
            case '?':
                exit_usage(NULL);
                break;
            default:
                exit_usage(NULL);
        }
        opt = getopt_long(argc, argv, "df:p:lcvi:", long_options, &optindex);
    }

    if (opt_p > 0) {
	if (db == NULL) {
	    int rc = sqlite3_open(get_dbname(), &db);
	    if (rc) {
		fprintf(stderr, "Error open DB %s", sqlite3_errmsg(db));
		sqlite3_close(db);
		return (1);
	    }
	}
	scan_mem(db, pid);
    }

    if (opt_v > 0) {
	// There should only be and exactly one of the l and c options provided for the -v option.
	if ((opt_l == 0 && opt_c == 0) || (opt_l > 0 && opt_c > 0)) {
	    exit_usage("-v requires either -l (library view) or -c (compartment view) first");
	}
	if (db == NULL) {
	    int rc = sqlite3_open(get_dbname(), &db);
	    if (rc) {
		fprintf(stderr, "Error open DB %s", sqlite3_errmsg(db));
		sqlite3_close(db);
		return (1);
	    }
	}
	// Library view
	if (opt_l > 0) {
	    xo_open_container("vm_view");
	    vm_caps_view(db);
	    xo_close_container("vm_view");
	}
	if (opt_c > 0) {
	    xo_open_container("compart_view");
	    // TODO: compart_caps_view(db);
	    xo_close_container("compart_view");
	}
    }
    if (opt_i > 0) {
	// There should only be and exactly one of the l and c options provided for the -v option.
	if ((opt_l == 0 && opt_c == 0) || (opt_l > 0 && opt_c > 0)) {
	    exit_usage("-v requires either -l (library view) or -c (compartment view) first");
	}
	if (db == NULL) {
	    int rc = sqlite3_open(get_dbname(), &db);
	    if (rc) {
		fprintf(stderr, "Error open DB %s", sqlite3_errmsg(db));
		sqlite3_close(db);
		return (1);
	    }
	}
	// Library view
	if (opt_l > 0) {
	    xo_open_container("caps_info_lib");
	    caps_syms_view(db, caps_info_param);
	    xo_close_container("caps_info_lib");
	}
	if (opt_c > 0) {
	    xo_open_container("caps_info_compart");
	    // TODO: caps_info_compart(db, caps_info_param);
	    xo_close_container("caps_info_compart");
	}

    }
    terminate_chericat(0);
}
