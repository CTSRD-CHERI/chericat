#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include <libxo/xo.h>
#include <cheri/cheric.h>

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
	fprintf(stdout, "\nUsage: chericat [[-d <database name>] [-s <pid>] [-v] [-m <binary name>]]\n"
			"     pid - pid of the target process for a snapshot of caps info\n"
			"     database name - name of the database to store data captured by chericat. If omitted, an in-memory db is used\n"
			"Options:\n"
			"     -s Scan the mapped memory and persist the caps data to a database\n"
			"     -v Show summary info of capabilities in the target process, arranged in mmap order\n"
			"     -m Show capabilities with corresponding symbols\n\n");
}

static struct option long_options[] = 
{
	{"scan_mem", required_argument, 0, 's'},
	{"caps_summary", no_argument, 0, 'v'},
	{"symbols_summary", required_argument, 0, 'm'},
	{"database_name", required_argument, 0, 'd'},
	{0,0,0,0}
};

int 
main(int argc, char *argv[])
{
	// libxo API to parse the libxo command line arguments. They are removed once parsed and stored, 
	// the program arguments would then be handled as intended without the libxo arguments.
	argc = xo_parse_args(argc, argv);


	// Create xo handle to store output to a file instead of the default stdout.
	FILE *fptr;
	// TODO - provide an option for user to provide file location
	fptr = fopen("/tmp/chericat_output.json", "w+");
	if (fptr == NULL) {
		fprintf(stderr, "file cannot be open");
		return 1;
	}

	xo_handle_t *xo_handle = xo_create_to_file(fptr, XO_STYLE_JSON, XOF_WARN|XOF_PRETTY);

	int optindex;
	int opt = getopt_long(argc, argv, "d:s:vm:", long_options, &optindex);

	if (opt == -1) {
		usage();
		return 0;
	}

	sqlite3 *db = NULL;

	while (opt != -1) {
		switch (opt)
		{
		case 's':
			if (db == NULL) {
        			int rc = sqlite3_open(get_dbname(), &db);

        			if (rc) {
                			fprintf(stderr, "Error open DB %s", sqlite3_errmsg(db));
              				sqlite3_close(db);
               		 		return (1);
        			}
			}
			scan_mem_using_procstat(db, optarg);
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
		case 'm':
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
		case 'd':
			dbname = (char*)malloc(strlen(optarg)+1);
			strcpy(dbname, optarg);
			break;
		default:
			usage();
			exit(0);
		}
		opt = getopt_long(argc, argv, "d:s:vm:", long_options, &optindex);
	}

	if (db != NULL) {
		sqlite3_close(db);
	}
        return 0;
}

