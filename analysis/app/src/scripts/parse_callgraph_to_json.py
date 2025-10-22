import sys
import json
from collections import defaultdict

def stream_callgraph_to_json_per_callsite(output_filepath, input_stream, min_group_occurrences=2):
    """
    Memory-efficient streaming parser that reads a callgraph and writes JSON.
    Groups callees per caller function and specific callsite.
    """
    func_to_id = {}
    func_info = {}   # full info for each function: name, module, addr
    func_counter = 1

    # key = (caller_fn, callsite)
    caller_to_callees = defaultdict(list)

    # --- Step 1: read edges and assign function IDs ---
    for line in input_stream:
        parts = line.strip().split()
        if len(parts) < 8:
            if line.strip():
                print(f"Skipping malformed line: {line.strip()}", file=sys.stderr)
            continue

        edge_type = parts[0]
        caller_module = parts[1]
        callsite = parts[2]
        caller_fn = parts[3]
        callee_addr = parts[4]
        callee_fn = parts[5]
        callee_module = parts[6]
        caller_addr = parts[7]

        # assign function IDs and store info
        for fn, mod, addr in [(caller_fn, caller_module, caller_addr), (callee_fn, callee_module, callee_addr)]:
            if fn not in func_to_id:
                fid = f"func_{func_counter}"
                func_to_id[fn] = fid
                func_info[fid] = {"id": fid, "name": fn, "module": mod, "addr": addr}
                func_counter += 1

        # append edge info keyed by (caller_fn, callsite)
        caller_to_callees[(caller_fn, callsite)].append({
            "callee_fn": callee_fn,
            "edge_type": edge_type
        })

    # --- Step 2: detect common callee groups per callsite ---
    callee_set_counts = defaultdict(int)
    for edges in caller_to_callees.values():
        callees_tuple = tuple(sorted(e["callee_fn"] for e in edges))
        callee_set_counts[callees_tuple] += 1

    callee_group_definitions = {}
    callee_groups = {}
    next_group_id = 1
    for callees_tuple, count in callee_set_counts.items():
        if count >= min_group_occurrences and len(callees_tuple) > 1:
            group_name = f"group_{next_group_id}"
            callee_group_definitions[callees_tuple] = group_name
            callee_groups[group_name] = [func_to_id[f] for f in callees_tuple]
            next_group_id += 1

    # --- Step 3: write JSON incrementally ---
    with open(output_filepath, 'w') as f:
        f.write('{\n')

        # Functions
        f.write('  "functions": [\n')
        funcs = sorted(func_info.values(), key=lambda x: x["id"])
        for i, info in enumerate(funcs):
            f.write(f'    {json.dumps(info)}')
            f.write(',\n' if i < len(funcs)-1 else '\n')
        f.write('  ],\n')

        # Callee groups
        f.write('  "callee_groups": {\n')
        for i, (group_name, callee_ids) in enumerate(callee_groups.items()):
            f.write(f'    "{group_name}": {json.dumps(callee_ids)}')
            f.write(',\n' if i < len(callee_groups)-1 else '\n')
        f.write('  },\n')

        # Call graph edges
        f.write('  "call_graph": [\n')
        total_edges = 0
        for edges in caller_to_callees.values():
            callees_tuple = tuple(sorted(e["callee_fn"] for e in edges))
            if callees_tuple in callee_group_definitions:
                total_edges += 1  # group counts as 1
            else:
                total_edges += len(edges)  # individual edges count separately
        
        written = 0
        for (caller_fn, callsite), edges in caller_to_callees.items():
            caller_id = func_to_id[caller_fn]
            callees_tuple = tuple(sorted(e["callee_fn"] for e in edges))

            # Check if this callsite's callee set is a group
            if callees_tuple in callee_group_definitions:
                edge_obj = {
                    "caller_id": caller_id,
                    "callee_type": "group",
                    "callee_ref": callee_group_definitions[callees_tuple],
                    "edge_type": edges[0]["edge_type"],  # assume same type
                    "callsite": callsite
                }
                f.write(f'    {json.dumps(edge_obj)}')
                written += 1
                f.write(',\n' if written < total_edges else '\n')
            else:
                for edge in edges:
                    edge_obj = {
                        "caller_id": caller_id,
                        "callee_type": "individual",
                        "callee_id": func_to_id[edge["callee_fn"]],
                        "edge_type": edge["edge_type"],
                        "callsite": callsite
                    }
                    f.write(f'    {json.dumps(edge_obj)}')
                    written += 1
                    f.write(',\n' if written < total_edges else '\n')

        f.write('  ]\n')
        f.write('}\n')

    print(f"Call graph saved to {output_filepath}", file=sys.stderr)


# --- Main ---
if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input_file_or_dash> <output_json>", file=sys.stderr)
        sys.exit(1)

    input_arg = sys.argv[1]
    output_file = sys.argv[2]

    if input_arg == "-":
        stream_callgraph_to_json_per_callsite(output_file, sys.stdin)
    else:
        with open(input_arg, 'r') as f:
            stream_callgraph_to_json_per_callsite(output_file, f)

