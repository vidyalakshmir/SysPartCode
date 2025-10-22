import sys
import argparse
import logging
import json
from collections import deque

class DirectedGraph:
    def __init__(self):
        self.graph = {}
        self.parents = {}
        self.nedges = 0;

    def add_node(self, node):
        if node not in self.graph:
            self.graph[node] = []
        if node not in self.parents:
            self.parents[node] = []

    def add_edge(self, src, dest, edge_type):
        self.add_node(src)
        self.add_node(dest)
        self.graph[src].append((dest, edge_type))
        self.parents[dest].append((src, edge_type))
        self.nedges = self.nedges + 1

    def get_neighbors(self, node):
        return self.graph.get(node, [])

    def get_parents(self, node):
        return self.parents.get(node, [])

    def print_edges(self):
        for src, neighbors in self.graph.items():
            for dest, edge_type in neighbors:
                print(f"{src} -[{edge_type}]-> {dest}")

graph = DirectedGraph()
targets = set()
startfuncs = set()

def generate_callgraph(filename):
    with open(filename, 'r') as f:
        for line in f:
            parts = line.strip().split()
            if len(parts) >= 8:
                edgetype = parts[0]
                src = parts[3] + "_" + parts[7]
                dest = parts[5] + "_" + parts[4]
                graph.add_edge(src,dest,edgetype);

def generate_callgraph_from_json(json_file):
    """
    Reads the callgraph JSON file and constructs a DirectedGraph object.
    
    Args:
        json_file (str): Path to the JSON callgraph.
    """
    with open(json_file, 'r') as f:
        data = json.load(f)
    
    # Build a mapping from function ID to function info
    func_id_to_info = {func["id"]: func for func in data["functions"]}
    
    # Process call graph edges
    for edge in data["call_graph"]:
        caller_id = edge["caller_id"]
        caller_info = func_id_to_info[caller_id]
        caller_str = f"{caller_info['name']}_{caller_info['addr']}"
        callsite = edge.get("callsite", "")
        
        if edge["callee_type"] == "individual":
            callee_id = edge["callee_id"]
            callee_info = func_id_to_info[callee_id]
            callee_str = f"{callee_info['name']}_{callee_info['addr']}"
            graph.add_edge(f"{caller_str}", callee_str, edge["edge_type"])
        elif edge["callee_type"] == "group":
            group_name = edge["callee_ref"]
            for callee_id in data["callee_groups"][group_name]:
                callee_info = func_id_to_info[callee_id]
                callee_str = f"{callee_info['name']}_{callee_info['addr']}"
                graph.add_edge(f"{caller_str}", callee_str, edge["edge_type"])


def read_syscalls(filename):
    with open(filename, 'r') as f:
        for line in f:
            parts = line.strip().split()
            if len(parts) >= 5:
                src = parts[0] + "_" + parts[1]
                syscall = parts[3]
                graph.add_edge(src,syscall,"syscall")
                targets.add(syscall)

def read_start_funcs(filename):
    with open(filename, 'r') as f:
        for line in f:
            parts = line.strip().split()
            if len(parts) >= 1:
                func = parts[0]
                startfuncs.add(func)

def find_reachable_syscalls(debug=False):
    visited = set()
    reachable_targets = set()
    parent = {}

    queue = deque(startfuncs)
    for s in startfuncs:
        visited.add(s)
        parent[s] = (None, None)

    while queue:
        node = queue.popleft()
        
        #if debug:
        #    print(f"Processing {node}")
        if node in targets:
            reachable_targets.add(node)
            if debug:
                path = []
                cur = node
                while cur is not None:
                    prev, edge_type = parent.get(cur, (None,None))
                    if prev is not None:
                        path.append(f"{prev} - [{edge_type}]-> {cur}")
                    cur = prev
                path.reverse()
                if debug:
                    logging.debug(f"\nPath to {node}:\n  " + "\n  ".join(path))

        for neighbor,edge_type in graph.get_neighbors(node):
            if neighbor not in visited:
                visited.add(neighbor)
                parent[neighbor] = (node, edge_type)
                queue.append(neighbor)
                #if debug:
                #    print(f" Processing edge : {node} -> {neighbor}")

    return list(reachable_targets)


def main():
    parser = argparse.ArgumentParser(description="Compute reachable syscalls from startfunc")
    parser.add_argument("args1", help="Callgraph file")
    parser.add_argument("args2", help="Syscalls with callsite info") 
    parser.add_argument("args3", help="file with list of start functions")
    parser.add_argument("--log", type=str, help="Enable logging")
    args = parser.parse_args()
    log_flag = False
    if args.log:
        log_flag = True
        logging.basicConfig(filename=args.log, 
                filemode='w',   # overwrite existing file 
                level=logging.DEBUG,
                format='%(message)s')
                #format='%(asctime)s - %(levelname)s - %(message)s')
    else:
        logging.basicConfig(level=logging.CRITICAL)  # suppress debug/info logs


    callgraphfile = args.args1
    syscallfile = args.args2
    startfuncfile = args.args3

    generate_callgraph(callgraphfile)
    generate_callgraph_from_json(callgraphfile)
    read_syscalls(syscallfile)
    read_start_funcs(startfuncfile)
    #graph.print_edges()
    reachable = find_reachable_syscalls(log_flag)
    
    for r in reachable:
        print(r)

if __name__ == "__main__":
    main()
