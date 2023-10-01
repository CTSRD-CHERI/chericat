#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

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
	fprintf(stdout, "\nUsage: chericat [-g <debug level>] [-d <database name>] [-p <pid>] [-v] [-c <binary name>]\n"
			"     debug level - 0 = No output; 1 = INFO; 2 = VERBOSE; 3 = TROUBLESHOOT\n"
			"     pid - pid of the target process for a snapshot of caps info\n"
			"     database name - name of the database to store data captured by chericat\n"
			"Options:\n"
			"     -g Determine the level of debugging messages to be printed. If omitted, the default is INFO level\n"
			"     -d Provide the database name to capture the data collected by chericat. If omitted, an in-memory db is used\n"
			"     -p Scan the mapped memory and persist the caps data to a database\n"
			"     -v Show virtual summary info of capabilities in the target process, arranged in mmap order\n"
			"     -c Show capabilities with corresponding symbols located in the provided binary\n\n");
}

static struct option long_options[] = 
{
	{"debug_level", required_argument, 0, 'g'},
	{"database_name", required_argument, 0, 'd'},
	{"scan_mem", required_argument, 0, 'p'},
	{"caps_summary", no_argument, 0, 'v'},
	{"caps_symbols_summary", required_argument, 0, 'c'},
	{0,0,0,0}
};

extern int print_level;

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
	int opt = getopt_long(argc, argv, "g:d:p:vc:", long_options, &optindex);

	if (opt == -1) {
		usage();
		return 0;
	}

	sqlite3 *db = NULL;

	print_level = INFO;

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
		case 'd':
			dbname = (char*)malloc(strlen(optarg)+1);
			strcpy(dbname, optarg);
			break;
		case 'g':
			print_level = atoi(optarg);
			break;
		default:
			usage();
			exit(0);
		}
		opt = getopt_long(argc, argv, "g:d:p:vc:", long_options, &optindex);
	}

	if (db != NULL) {
		sqlite3_close(db);
	}
        return 0;
}

