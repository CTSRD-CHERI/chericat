import graphviz
import json
import sqlite3
import sys, os, argparse
import math
from IPython.display import display

parser = argparse.ArgumentParser(prog='chericat_cli')
parser.add_argument(
	'-d', 
	help='The database to use for the queries', 
    required=True,
)

parser.add_argument(
	'-g', 
	help='Binary Graph', 
	action='store_true',
)

parser.add_argument(
	'-l', 
	help='Show capabilities referencing the provided library <path>',
	nargs=1,
)
    
parser.add_argument(
	'-q', 
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
	#_binary_graph(db, digraph)
	digraph.render(directory='graph-output', view=True)  
	print("args.g")

if args.l:
	digraph = graphviz.Digraph('G', filename='graph_for_'+args.l[0]+'.gv')
	#_show_caps_to_bin(db, args.l[0], digraph)
	digraph.render(directory='graph-output', view=True)  
	print("args.l")

if args.q:
	#print(_run_sql_query(db, args.q[0]))
	print("args.q")

if args.c:
	digraph = graphviz.Digraph('G', filename=args.c[0]+'_vs_'+args.c[1]+'.gv')
	#_show_caps_between_two_libs(db, args.c[0], args.c[1], digraph)
	digraph.render(directory='graph-output', view=True)
	print("args.c")

