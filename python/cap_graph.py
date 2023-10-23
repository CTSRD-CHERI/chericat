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

import math

import db_utils
import gv_utils

def show_caps_to_bin(db, path, graph):
    get_caps_q = "SELECT * FROM cap_info"
    caps = db_utils.run_sql_query(db, get_caps_q)
    
    lib_start_q = "SELECT start_addr FROM vm WHERE mmap_path LIKE '%" + str(path) + "%'"
    lib_end_q = "SELECT end_addr FROM vm WHERE mmap_path LIKE '%" + str(path) + "%'"
    lib_start_addrs = db_utils.run_sql_query(db, lib_start_q)
    lib_end_addrs = db_utils.run_sql_query(db, lib_end_q)
    
    # node showing the loaded library, with cap records pointing into it
    # We have all the information here at this point, just need to graph it
    
    # First we create the library node
    nodes = []
    edges = []
    
    nodes.append(gv_utils.gen_node(path, path, "lightblue"))
    
    for cap in caps:
        cap_loc_addr = cap[0] 
        cap_path = cap[1]
        cap_addr = cap[2]
        cap_perms = cap[3]
        cap_base = cap[4]
        cap_top = cap[5]
        
        if path in cap_path:
            node_label = cap_addr
            cap_node = gv_utils.gen_node(cap_addr, cap_addr +"|"+cap_perms+"|"+cap_base+"-"+cap_top, "pink")
            nodes.append(cap_node)

    gv_utils.gen_records(graph, nodes, edges)

def show_caps_between_two_libs(db, lib1, lib2, graph):
    lib1_caps_q = "SELECT * FROM cap_info WHERE cap_loc_path LIKE '%" + str(lib1) + "%'"
    lib1_caps = db_utils.run_sql_query(db, lib1_caps_q)
    
    lib2_caps_q = "SELECT * FROM cap_info WHERE cap_loc_path LIKE '%" + str(lib2) + "%'"
    lib2_caps = db_utils.run_sql_query(db, lib2_caps_q)
    
    lib1_start_q = "SELECT start_addr FROM vm WHERE mmap_path LIKE '%" + str(lib1) + "%'"
    lib1_end_q = "SELECT end_addr FROM vm WHERE mmap_path LIKE '%" + str(lib1) + "%'"
    lib1_start_addrs = db_utils.run_sql_query(db, lib1_start_q)
    lib1_end_addrs = db_utils.run_sql_query(db, lib1_end_q)
    
    lib2_start_q = "SELECT start_addr FROM vm WHERE mmap_path LIKE '%" + str(lib2) + "%'"
    lib2_end_q = "SELECT end_addr FROM vm WHERE mmap_path LIKE '%" + str(lib2) + "%'"
    lib2_start_addrs = db_utils.run_sql_query(db, lib2_start_q)
    lib2_end_addrs = db_utils.run_sql_query(db, lib2_end_q)
    
    # node showing the loaded libraries, with cap records pointing into it
    # We have all the information here at this point, just need to graph it
    
    # First we create the two library nodes
    nodes = []
    edges = []
    
    nodes.append(gv_utils.gen_node(lib1, lib1, "lightblue"))
    nodes.append(gv_utils.gen_node(lib2, lib2, "pink"))
    
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
                
            # Test if cap1's pointer address is in lib2's address range
            if cap_addr >= lib2_start_addr and cap_addr <= lib2_end_addr:
                for props in edges:
                    if props.get("src") == lib1 and props.get("dest") == lib2 and props.get("label") == cap_perms:
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

            # Test if cap2's pointer address is in lib1's address range
            if cap_addr >= lib1_start_addr and cap_addr <= lib1_end_addr:
                for props in edges:
                    if props.get("src") == lib2 and props.get("dest") == lib1 and props.get("label") == cap_perms:
                        penwidth_weight = math.log((10**(float(props.get("penwidth"))) + 1), 10)
                        edges.remove(props)
                        break
                edges.append({"src":lib2, "dest":lib1, "label":cap_perms, "penwidth":str(penwidth_weight)})

    gv_utils.gen_records(graph, nodes, edges)
