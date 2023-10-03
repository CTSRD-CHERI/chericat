import graphviz
import json

def gen_records(graph, nodes, edge_dict_list):
    graph.attr('node', shape='record', fontname='Courier', size='6,6')
    graph.node_attr['fontname'] = "Courier"
    graph.node_attr['color'] = "lightgreen"
    graph.node_attr['fillcolor'] = "lightblue"
    graph.node_attr['style'] = 'filled'
    graph.edge_attr['fontname'] = "Courier"
    graph.graph_attr['rankdir'] = "RL"
    graph.node_attr['fontsize'] = "10"
        
    for node in nodes:
        graph.node(node.get("id"), node.get("txt"), fillcolor=node.get("fillcolor"))
        
    edges = []
    for edge_dict in edge_dict_list:
        graph.edge(
            edge_dict.get("src"), 
            edge_dict.get("dest"), 
            label=edge_dict.get("label"), 
            penwidth=edge_dict.get("penwidth")
        )
    
def gen_node(node_id, node_txt, fillcolor):
    node = {}
    node["id"] = node_id
    node["txt"] = node_txt
    node["fillcolor"] = fillcolor
    return node

