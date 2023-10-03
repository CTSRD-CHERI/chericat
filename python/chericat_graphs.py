import graphviz
import sys, os, argparse

import db_utils
import full_graph
import cap_graph

parser = argparse.ArgumentParser(prog='chericat_cli')
parser.add_argument(
	'-d', 
	help='The database to use for the queries', 
    required=True,
)

parser.add_argument(
	'-g', 
	help='Generate full capability relationship in mmap graph', 
	action='store_true',
)

parser.add_argument(
	'-l', 
	help='Show capabilities referencing the provided library <libname>',
	nargs=1,
)
    
parser.add_argument(
	'-r', 
	help='Executes the SQL query on the provided db',
	nargs=1,
)
    
parser.add_argument(
	'-c',
	help="Show capabilities between two loaded libraries <libname 1> <libname 2>",
	nargs=2,
)

args = parser.parse_args()

if args.d:
	db = args.d

if args.g:
	digraph = graphviz.Digraph('G', filename='graph_overview.gv')
	full_graph.gen_full_graph(db, digraph)
	digraph.render(directory='graph-output', view=True)  

if args.l:
	digraph = graphviz.Digraph('G', filename='graph_for_'+args.l[0]+'.gv')
	cap_graph.show_caps_to_bin(db, args.l[0], digraph)
	digraph.render(directory='graph-output', view=True)  

if args.r:
	print(db_utils.run_sql_query(db, args.q[0]))

if args.c:
	digraph = graphviz.Digraph('G', filename=args.c[0]+'_vs_'+args.c[1]+'.gv')
	cap_graph.show_caps_between_two_libs(db, args.c[0], args.c[1], digraph)
	digraph.render(directory='graph-output', view=True)

