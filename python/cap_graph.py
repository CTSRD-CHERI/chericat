import math

import gen_gv_records

def show_caps_to_bin(db, path, graph):
    get_caps_q = "SELECT * FROM cap_info"
    caps = gen_gv_records.run_sql_query(db, get_caps_q)
    
    lib_start_q = "SELECT start_addr FROM vm WHERE mmap_path LIKE '%" + str(path) + "%'"
    lib_end_q = "SELECT end_addr FROM vm WHERE mmap_path LIKE '%" + str(path) + "%'"
    lib_start_addrs = gen_gv_records.run_sql_query(db, lib_start_q)
    lib_end_addrs = gen_gv_records.run_sql_query(db, lib_end_q)
    
    # node showing the loaded library, with cap records pointing into it
    # We have all the information here at this point, just need to graph it
    
    # First we create the library node
    nodes = []
    edges = []
    
    nodes.append(gen_gv_records.gen_node(path, path, "lightblue"))
    
    for cap in caps:
        cap_loc_addr = cap[0] 
        cap_path = cap[1]
        cap_addr = cap[2]
        cap_perms = cap[3]
        cap_base = cap[4]
        cap_top = cap[5]
        
        if path in cap_path:
            node_label = cap_addr
            cap_node = gen_gv_records.gen_node(cap_addr, cap_addr +"|"+cap_perms+"|"+cap_base+"-"+cap_top, "pink")
            nodes.append(cap_node)

    gen_gv_records.gen_records(graph, nodes, edges)

def show_caps_between_two_libs(db, lib1, lib2, graph):
    lib1_caps_q = "SELECT * FROM cap_info WHERE cap_loc_path LIKE '%" + str(lib1) + "%'"
    lib1_caps = gen_gv_records.run_sql_query(db, lib1_caps_q)
    
    lib2_caps_q = "SELECT * FROM cap_info WHERE cap_loc_path LIKE '%" + str(lib2) + "%'"
    lib2_caps = gen_gv_records.run_sql_query(db, lib2_caps_q)
    
    lib1_start_q = "SELECT start_addr FROM vm WHERE mmap_path LIKE '%" + str(lib1) + "%'"
    lib1_end_q = "SELECT end_addr FROM vm WHERE mmap_path LIKE '%" + str(lib1) + "%'"
    lib1_start_addrs = gen_gv_records.run_sql_query(db, lib1_start_q)
    lib1_end_addrs = gen_gv_records.run_sql_query(db, lib1_end_q)
    
    lib2_start_q = "SELECT start_addr FROM vm WHERE mmap_path LIKE '%" + str(lib2) + "%'"
    lib2_end_q = "SELECT end_addr FROM vm WHERE mmap_path LIKE '%" + str(lib2) + "%'"
    lib2_start_addrs = gen_gv_records.run_sql_query(db, lib2_start_q)
    lib2_end_addrs = gen_gv_records.run_sql_query(db, lib2_end_q)
    
    # node showing the loaded libraries, with cap records pointing into it
    # We have all the information here at this point, just need to graph it
    
    # First we create the two library nodes
    nodes = []
    edges = []
    
    nodes.append(gen_gv_records.gen_node(lib1, lib1, "lightblue"))
    nodes.append(gen_gv_records.gen_node(lib2, lib2, "pink"))
    
    for cap1 in lib1_caps:
        cap_loc_addr = cap1[0] 
        cap_path = cap1[1]
        cap_addr = cap1[2]
        cap_perms = cap1[3]
        cap_base = cap1[4]
        cap_top = cap1[5]
        
        penwidth_weight = 1;
        
        for lib2_addr_index in range(len(lib2_start_addrs)):
            lib2_start_addr = lib2_start_addrs[lib2_addr_index][0]
            lib2_end_addr = lib2_end_addrs[lib2_addr_index][0]
                
            if cap_addr >= lib2_start_addr and cap_addr <= lib2_end_addr:
                for props in edges:
                    if props.get("src") == lib1 and props.get("dest") == lib2:
                        penwidth_weight = math.log((10**(float(props.get("penwidth"))) + 1), 10)
                        edges.remove(props)
                        break
                edges.append({"src":lib1, "dest":lib2, "label":cap_perms, "penwidth":str(penwidth_weight)})
  
    for cap2 in lib2_caps:
        cap_loc_addr = cap2[0] 
        cap_path = cap2[1]
        cap_addr = cap2[2]
        cap_perms = cap2[3]
        cap_base = cap2[4]
        cap_top = cap2[5]
        
        for lib1_addr_index in range(len(lib1_start_addrs)):
            lib1_start_addr = lib1_start_addrs[lib1_addr_index][0]
            lib1_end_addr = lib1_end_addrs[lib1_addr_index][0]
                
            if cap_addr >= lib1_start_addr and cap_addr <= lib1_end_addr:
                for props in edges:
                    if props.get("src") == lib2 and props.get("dest") == lib1:
                        penwidth_weight = math.log((10**(float(props.get("penwidth"))) + 1), 10)
                        edges.remove(props)
                        break
                edges.append({"src":lib2, "dest":lib1, "label":cap_perms, "penwidth":str(penwidth_weight)})

    gen_gv_records.gen_records(graph, nodes, edges)
