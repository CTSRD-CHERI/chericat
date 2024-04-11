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
import json

def gen_records(graph, nodes, edge_dict_list):
    graph.attr('node', shape='record', fontname='Courier', size='6,6')
    graph.node_attr['fontname'] = "Courier"
    graph.node_attr['color'] = "lightgreen"
    graph.node_attr['fillcolor'] = "lightblue"
    graph.node_attr['style'] = 'filled'
    graph.edge_attr['fontname'] = "Courier"
    graph.graph_attr['rankdir'] = "TB"
    graph.node_attr['fontsize'] = "10"
        
    for node in nodes:
        graph.node(node.get("id"), 
                   node.get("txt"), 
                   fillcolor=node.get("fillcolor"),
                   rank=node.get("rank"))
        
    edges = []
    for edge_dict in edge_dict_list:
        graph.edge(
            edge_dict.get("src"), 
            edge_dict.get("dest"), 
            label=edge_dict.get("label"), 
            penwidth=edge_dict.get("penwidth")
        )
    
def gen_node(node_id, node_txt, fillcolor, rank):
    node = {}
    node["id"] = node_id
    node["txt"] = node_txt
    node["fillcolor"] = fillcolor
    node["rank"] = rank
    return node

