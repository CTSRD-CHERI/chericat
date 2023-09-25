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
#include "data_presentation.h"

/*
 * usage()
 * Useful message to users to show how to run the program and what parameters are accepted.
 */

static void usage()
{
	fprintf(stdout, "\nUsage: chericat [[-d <database name>] [-s <pid>] [-v] [-n <binary name>] [-m]]\n"
			"     pid - pid of the target process for a snapshot of caps info\n"
			"     database name - name of the database to store data captured by chericat\n"
			"Options:\n"
			"     -s Scan the mapped memory and persist the caps data to a database\n"
			"     -v Show summary info of capabilities in the target process, arranged in mmap order\n"
			"     -n Show capabilities in the loaded binary <binary name>\n"
			"     -m Show capabilities with corresponding symbols\n\n");
}

static struct option long_options[] = 
{
	{"scan_mem", required_argument, 0, 's'},
	{"caps_summary", no_argument, 0, 'v'},
	{"caps_for_lib", required_argument, 0, 'n'},
	{"symbols_summary", no_argument, 0, 'm'},
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
	int opt = getopt_long(argc, argv, "d:s:vn:m", long_options, &optindex);

	if (opt == -1) {
		usage();
		return 0;
	}

	while (opt != -1) {
		switch (opt)
		{
		case 's':
			scan_mem(optarg);
			//read_lwps(optarg);
			//get_vmmap_from_procstat(optarg);
			break;
		case 'v':
			xo_open_list("vm_cap_output");
			vm_caps_view();
			xo_close_list("vm_cap_output");
			break;
		case 'n':
			printf("View of caps within address range of library %s\n", optarg);
			break;
		case 'm':
	       		xo_open_list("cap_sym_output");
			cap_sym_view();
			xo_close_list("cap_sym_output");
			break;
		case 'd':
			dbname = (char*)malloc(strlen(optarg)+1);
			strcpy(dbname, optarg);
			break;
		default:
			usage();
		}
		opt = getopt_long(argc, argv, "d:s:vn:m", long_options, &optindex);
	}

        return 0;
}

