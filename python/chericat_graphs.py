#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2023 Jessica Man
#
# This software was developed by the University of Cambridge Computer
# Laboratory (Department of Computer Science and Technology) as part of the
# CHERI for Hypervisors and Operating Systems (CHaOS) project, funded by
# EPSRC grant EP/V000292/1.

#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

import graphviz
import sys, os, argparse
import time

import db_utils
import full_graph
import cap_graph
import comparts_graph

toplevel_parser = argparse.ArgumentParser(prog='chericat_visualise')
sub_parsers = toplevel_parser.add_subparsers(title='Views', help='libview or compview', dest='chosen_view')

libview_parser = sub_parsers.add_parser('libview', add_help=False, help='Sub-commands for library-centric graphs')
libview_parser.add_argument(
	'-d', 
	help='The database to use for the queries', 
	required=True,
)
libview_parser.add_argument(
	'-r', 
	help='Executes the SQL query on the provided db',
	nargs=1,
)

libview_parser.add_argument(
	'-g', 
	help='Generate overview graph showing capability relationships between libraries', 
	action='store_true',
)
libview_parser.add_argument(
	'-c',
	help="Show capabilities between two loaded libraries <libname 1> <libname 2>",
	nargs=2,
)

compview_parser = sub_parsers.add_parser('compview', add_help=False, help='Sub-commands for compartment-centric graphs')
compview_parser.add_argument(
	'-d', 
	help='The database to use for the queries', 
	required=True,
)
compview_parser.add_argument(
	'-r', 
	help='Executes the SQL query on the provided db',
	nargs=1,
)

compview_parser.add_argument(
	'-g', 
	help='Generate overview graph showing capability relationships between compartments', 
	action='store_true',
)

compview_parser.add_argument(
	'-g_no_perms', 
	help="Generate overview graph showing capability relationships (but don't show permissions) between compartments", 
	action='store_true',
)

compview_parser.add_argument(
	'-c',
	help="Show capabilities between two compartments <compname 1> <compname 2>",
	nargs=2,
)

args = toplevel_parser.parse_args()

if args.d:
	db = args.d
	dbname = os.path.basename(db)

if args.r:
	print(db_utils.run_sql_query(db, args.r[0]))

if args.g and args.chosen_view == 'libview':
	digraph = graphviz.Digraph('G', filename=dbname+'.libview_full_graph.gv')
	full_graph.gen_full_graph(db, digraph)
	digraph.render(directory='graph-output', view=True)  

if args.c and args.chosen_view == 'libview':
	digraph = graphviz.Digraph('G', filename='libview_'+args.c[0]+'_vs_'+args.c[1]+'.gv')
	cap_graph.show_caps_between_two_libs(db, args.c[0], args.c[1], digraph)
	digraph.render(directory='graph-output', view=True)

if args.g and args.chosen_view=='compview':
	start = time.perf_counter()
	digraph = graphviz.Digraph('G', filename=dbname+'.compart_full_graph.gv')
	comparts_graph.show_comparts(db, digraph)
	end = time.perf_counter()
	print("Compartments graph generation time taken: " + str(end-start) + "s")
	digraph.render(directory='graph-output', view=True)

if args.chosen_view == 'compview' and args.g_no_perms:
	start = time.perf_counter()
	digraph = graphviz.Digraph('G', filename=dbname+'.compart_no_perms_graph.gv')
	comparts_graph.show_comparts_no_perms_caps(db, digraph)
	end = time.perf_counter()
	print("Compartments graph (no perms) generation time taken: " + str(end-start) + "s")
	digraph.render(directory='graph-output', view=True)
	
