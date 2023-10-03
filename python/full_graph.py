import json
import math
import sqlite3

import db_utils
import gv_utils

def gen_full_graph(db, graph):
    get_bin_paths_q = "SELECT DISTINCT mmap_path FROM vm"
    path_list_json = db_utils.run_sql_query(db, get_bin_paths_q)
    
    get_caps_q = "SELECT * FROM cap_info"
    caps = db_utils.run_sql_query(db, get_caps_q)
                
    nodes = []
    edges = []
    
    for path_list in path_list_json:
        if (path_list[0] == "unknown" or 
            path_list[0] == "Stack" or 
            path_list[0] == "Guard"):
            
            get_start_addr_q = "SELECT start_addr FROM vm WHERE mmap_path=\"" + path_list[0] + "\""
            # Only interested in the first result?
            start_addr_list_json = db_utils.run_sql_query(db, get_start_addr_q)
            path_label = path_list[0] + " (" + start_addr_list_json[0][0] + ")"
            fillcolor = "lightgrey"
        else:
            path_label = path_list[0]
            fillcolor = "lightblue"

        nodes.append(gv_utils.gen_node(path_label, path_label, fillcolor))
        
        lib_start_q = "SELECT start_addr FROM vm WHERE mmap_path LIKE '%" + str(path_list[0]) + "%'"
        lib_end_q = "SELECT end_addr FROM vm WHERE mmap_path LIKE '%" + str(path_list[0]) + "%'"
        lib_start_addrs = db_utils.run_sql_query(db, lib_start_q)
        lib_end_addrs = db_utils.run_sql_query(db, lib_end_q)
        
        for cap in caps:
            cap_loc_addr = cap[0] 
            cap_path = cap[1]
            cap_addr = cap[2]
            cap_perms = cap[3]
            cap_base = cap[4]
            cap_top = cap[5]
            
            penwidth_weight = 1

            for lib_addr_index in range(len(lib_start_addrs)):
                lib_start_addr = lib_start_addrs[lib_addr_index][0]
                lib_end_addr = lib_end_addrs[lib_addr_index][0]
                
                if cap_addr >= lib_start_addr and cap_addr <= lib_end_addr and cap_path != path_list[0]:
                    
                    if (cap_path == "unknown" or 
                        cap_path == "Stack" or 
                        cap_path == "Guard"):
                        
                        get_start_addr_q = "SELECT start_addr FROM vm WHERE mmap_path=\"" + cap_path + "\""
                        # Only interested in the first result?
                        start_addr_list_json = db_utils.run_sql_query(db, get_start_addr_q)
                        cap_path_label = cap_path + " (" + start_addr_list_json[0][0] + ")"
                    else:
                        cap_path_label = cap_path
                    
                    for props in edges:
                        if props.get("src") == cap_path_label and props.get("dest") == path_label and props.get("label") == cap_perms:
                            penwidth_weight = math.log((10**(float(props.get("penwidth"))) + 1), 10)
                            edges.remove(props)
                            break
                    edges.append({"src":cap_path_label, "dest":path_label, "label":cap_perms, "penwidth":str(penwidth_weight)})
    gv_utils.gen_records(graph, nodes, edges)

