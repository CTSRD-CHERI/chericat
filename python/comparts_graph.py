#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2023 Capabilities Limited
#
# This software was developed by Capabilities Limited under Innovate UK
# project 10027440, "Developing and Evaluating an Open-Source Desktop for Arm
# Morello".
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

import json
import math
import sqlite3
import os

import db_utils
import gv_utils

def show_comparts(db, graph):
    get_compart_id_q = "SELECT DISTINCT compart_id FROM vm"
    compart_ids = db_utils.run_sql_query(db, get_compart_id_q)
    
    get_caps_q = "SELECT * FROM cap_info"
    caps = db_utils.run_sql_query(db, get_caps_q)

    nodes = []
    edges = []

    for compart_id in compart_ids:
        single_compart_id = compart_id[0]
        get_path_for_id_q = "SELECT DISTINCT mmap_path FROM vm WHERE compart_id=" + str(single_compart_id)
        paths = db_utils.run_sql_query(db, get_path_for_id_q)

        path = ""
        count = 0
        for each_path in paths:
            path += each_path[0]
            if (count != len(paths)-1):
                path += ", "
            count += 1
            
        if (single_compart_id < 0):
            fillcolor = "lightgrey"
            rank = "source"
        else:
            fillcolor = "lightblue"
            rank = "max"
        nodes.append(gv_utils.gen_node(
                        str(single_compart_id), 
                        str(single_compart_id)+": "+path, 
                        fillcolor,
                        rank))
        
        for path_list in paths:
            path_label = path_list[0]

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

                    # Also remove the appended (.bss/got/plt) and compare
                    if cap_addr >= lib_start_addr and \
                        cap_addr <= lib_end_addr and \
                        cap_path != path_list[0] and \
                        cap_path != path_list[0][:-6] and \
                        cap_path[:-6] != path_list[0] and \
                        cap_path[:-6] != path_list[0][:-6]:
                        find_compart_id_q = "SELECT DISTINCT compart_id FROM vm WHERE mmap_path LIKE '%" + cap_path + "%'"
                        cap_path_compart_id_json = db_utils.run_sql_query(db, find_compart_id_q)
                        # only need the first compart_id as they should be all the same 
                        cap_compart_id = cap_path_compart_id_json[0][0]
                        
                        for props in edges:
                            if props.get("src") == str(cap_compart_id) and props.get("dest") == str(single_compart_id) and props.get("label") == cap_perms:
                                penwidth_weight = math.log((10**(float(props.get("penwidth"))) + 1), 10)
                                edges.remove(props)
                                break

                        if (str(cap_compart_id) != str(single_compart_id)):
                                edges.append({"src":str(cap_compart_id), "dest":str(single_compart_id), "label":cap_perms, "penwidth":str(penwidth_weight)})

    gv_utils.gen_records(graph, nodes, edges)

def show_comparts_no_perms_caps(db, graph):
    get_compart_id_q = "SELECT distinct compart_id, mmap_path FROM vm"
    compart_ids = db_utils.run_sql_query(db, get_compart_id_q) # returns an array of arrays, each with 2 elements: compart_id and mmap_path
    get_caps_q = "SELECT * FROM cap_info"
    caps = db_utils.run_sql_query(db, get_caps_q)
    
    nodes = []
    edges = []

    for results in compart_ids:
        compart_id = results[0]
        mmap_path = os.path.basename(results[1])

        #print(str(compart_id) + ":" + mmap_path)

        if (compart_id < 0):
            fillcolor = "lightgrey"
            rank = "source"
        else:
            fillcolor = "lightblue"
            rank = "max"
            
        nodes.append(gv_utils.gen_node(
                        str(compart_id), 
                        str(compart_id)+": "+mmap_path, 
                        fillcolor,
                        rank))

        lib_start_q = "SELECT start_addr FROM vm WHERE mmap_path LIKE '%" + mmap_path + "%'"
        lib_end_q = "SELECT end_addr FROM vm WHERE mmap_path LIKE '%" + mmap_path + "%'"
        lib_start_addrs = db_utils.run_sql_query(db, lib_start_q)
        lib_end_addrs = db_utils.run_sql_query(db, lib_end_q)    

        for cap in caps:
            cap_loc_addr = cap[0]
            cap_path = cap[1]
            cap_addr = cap[2]
            cap_perms = cap[3]
            cap_base = cap[4]
            cap_top = cap[5]
 
            for lib_addr_index in range(len(lib_start_addrs)):
                lib_start_addr = lib_start_addrs[lib_addr_index][0]
                lib_end_addr = lib_end_addrs[lib_addr_index][0]      

                # Also remove the appended (.got/.plt) and compare
                if cap_addr >= lib_start_addr and \
                    cap_addr <= lib_end_addr and \
                    cap_path != mmap_path and \
                    cap_path != mmap_path[:-6] and \
                    cap_path[:-6] != mmap_path and \
                    cap_path[:-6] != mmap_path[:-6]:

                    find_compart_id_q = "SELECT DISTINCT compart_id FROM vm WHERE mmap_path LIKE '%" + cap_path + "%'"
                    cap_path_compart_id_json = db_utils.run_sql_query(db, find_compart_id_q)
                    # only need the first compart_id as they should be all the same 
                    cap_compart_id = cap_path_compart_id_json[0][0]
#                   #print("cap_path_label: " + str(cap_compart_id) + " single_compart_id: " + str(single_compart_id) + " compart path: " + path)
               
                    for props in edges:
                        if props.get("src") == str(cap_compart_id) and props.get("dest") == str(compart_id):
                            # penwidth_weight = math.log((10**(float(props.get("penwidth"))) + 1), 10)
                            edges.remove(props)
                            break
                    if (str(cap_compart_id) != str(compart_id)):
                        #edges.append({"src":str(cap_compart_id), "dest":str(compart_id), "label":"", "penwidth":1})
                        edges.append({"src":str(cap_compart_id), "dest":str(compart_id), "label":""})
        
    gv_utils.gen_records(graph, nodes, edges)

def show_caps_between_two_comparts(db, c1, c2, graph):
    
    # Get all the cap_addr (out) from the source compartment (for both c1 and c2)
    dest_caps_from_src1_q = "SELECT cap_addr, perms FROM cap_info INNER JOIN vm ON cap_info.cap_loc_path = vm.mmap_path where vm.compart_id="+c1;
    dest_caps_from_src1_json = db_utils.run_sql_query(db, dest_caps_from_src1_q)

    dest_caps_from_src2_q = "SELECT cap_addr, perms FROM cap_info INNER JOIN vm ON cap_info.cap_loc_path = vm.mmap_path where vm.compart_id="+c2;
    dest_caps_from_src2_json = db_utils.run_sql_query(db, dest_caps_from_src2_q)
    
    # Get the start and end addr to get the addr range to compart cap_addr with, to find out if any caps from
    # c1 are pointing to c2 and vice versa
    addr_ranges_c1_as_dest_q = "SELECT start_addr, end_addr FROM vm WHERE compart_id=" + c1
    addr_ranges_c1_as_dest_json = db_utils.run_sql_query(db, addr_ranges_c1_as_dest_q)

    addr_ranges_c2_as_dest_q = "SELECT start_addr, end_addr FROM vm WHERE compart_id=" + c2
    addr_ranges_c2_as_dest_json = db_utils.run_sql_query(db, addr_ranges_c2_as_dest_q)

    # We have all the information here at this point, just need to graph it

    # First we create the two comparts nodes
    nodes = []
    edges = []
    
    # Get the paths for each compartment node
    c1_paths_q = "SELECT distinct mmap_path FROM vm WHERE compart_id=" + c1
    c1_paths_json = db_utils.run_sql_query(db, c1_paths_q)
    
    c2_paths_q = "SELECT distinct mmap_path FROM vm WHERE compart_id=" + c2
    c2_paths_json = db_utils.run_sql_query(db, c2_paths_q)

    path1 = ""
    count = 0
    for c1_path in c1_paths_json:
        path1 += os.path.basename(c1_path[0])
        if (count != len(c1_paths_json)-1):
            path1 += ", "
        count += 1    
    path2 = ""
    count = 0
    for c2_path in c2_paths_json:
        path2 += os.path.basename(c2_path[0])
        if (count != len(c2_paths_json)-1):
            path2 += ", "
        count += 1  
        
    print(path1)
    print(path2)
    nodes.append(gv_utils.gen_node(c1, c1+":"+path1, "lightblue", "same"))
    nodes.append(gv_utils.gen_node(c2, c2+":"+path2, "pink", "same"))
    
    # Iterate through each cap_addr from source c1 and find out if any of them point to c2
    for addr_ranges_c2_as_dest in addr_ranges_c2_as_dest_json:
        penwidth_weight = 1;
        
        start_addr = addr_ranges_c2_as_dest[0]
        end_addr = addr_ranges_c2_as_dest[1]
        
        print("src:"+c1+" dest:"+c2+" dest addr range:"+start_addr+"-"+end_addr)
        
        for dest_cap_from_src1 in dest_caps_from_src1_json:
            dest_cap = dest_cap_from_src1[0]
            dest_perms = dest_cap_from_src1[1]
            
            # Now check if any of the dest caps are within the range of the dest address range
            if dest_cap >= start_addr and dest_cap <= end_addr:
                for props in edges:
                    if (props.get("src") == c1 and props.get("dest") == c2):
                        penwidth_weight = math.log((10**(float(props.get("penwidth"))) + 1), 10)
                        edges.remove(props)
                        break
                edges.append({"src":c1, "dest":c2, "label":dest_perms, "penwidth":str(penwidth_weight)})
    
    # Iterate through each cap_addr from source c2 and find out if any of them point to c1
    for addr_ranges_c1_as_dest in addr_ranges_c1_as_dest_json:
        penwidth_weight = 1;
        
        start_addr = addr_ranges_c1_as_dest[0]
        end_addr = addr_ranges_c1_as_dest[1]
                
        for dest_cap_from_src2 in dest_caps_from_src2_json:
            dest_cap = dest_cap_from_src2[0]
            dest_perms = dest_cap_from_src2[1]
            
            # Now check if any of the dest caps are within the range of the dest address range
            if dest_cap >= start_addr and dest_cap <= end_addr:
                for props in edges:
                    if (props.get("src") == c2 and props.get("dest") == c1):
                        penwidth_weight = math.log((10**(float(props.get("penwidth"))) + 1), 10)
                        edges.remove(props)
                        break
                edges.append({"src":c2, "dest":c1, "label":dest_perms, "penwidth":str(penwidth_weight)})

    gv_utils.gen_records(graph, nodes, edges)
