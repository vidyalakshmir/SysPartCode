#!/bin/bash

show_help() {
    cat << EOF
Usage: $0 arg1 arg2 arg3 [--log]

Arguments:
  arg1    ELF binary to be analysed
  arg2    Folder to store outputs
  arg3    Path to a file containing one or more start functions (one per line)

Options:
  --log   This will log the paths to the system calls from the start functions in logfile.txt in the output folder
  -h, --help
          Show this help message and exit

Outputs:
  syscalls.txt			List of system calls reachable from the start functions
  callgraph.txt			Callgraph of the ELF binary and its libraries
  syscalls_with_callsites.txt	The functions within the call graph where system calls are invoked
  allfunctions.txt		List of all functions in ELF binary and its libraries along with their addresses
  logfile.txt			Contains paths to the system calls from the start functions
EOF
}

if [[ $# -lt 3 || "$1" == "-h" || "$1" == "--help" ]]; then
    show_help
    exit 0
fi

APP=$1
OUT=$(realpath $2)
STARTFILE=$(realpath $3)
shift 3

log_flag=false
while [[ $# -gt 0 ]]; do
    case $1 in
        --log) log_flag=true ;;
    esac
    shift
done

temp_start_func=$(head -1 $STARTFILE)
./syspart -p $APP -s $temp_start_func -a 25 > $OUT/allfunctions.txt
./syspart -p $APP -s $temp_start_func -a 24 > $OUT/syscalls_with_callsites.txt
./syspart -p $APP -s $STARTFILE -i -a 23 | python3 src/scripts/parse_callgraph_to_json.py - $OUT/callgraph.json

rm "${OUT}/startfuncs_with_addr.txt"
while read -r START_FUNC; do
    start_addr=$(awk -v fname="$START_FUNC" '$1 == fname {print $2}' "$OUT/allfunctions.txt")
    if [ -n "$start_addr" ]; then
        echo "${START_FUNC}_${start_addr}" >> "${OUT}/startfuncs_with_addr.txt"
    fi
done < "$STARTFILE"
if [ "$log_flag" = true ]; then
	python3 src/scripts/getsyscalls.py $OUT/callgraph.json $OUT/syscalls_with_callsites.txt $OUT/startfuncs_with_addr.txt --log $OUT/logfile.txt > $OUT/syscalls.txt
else
	python3 src/scripts/getsyscalls.py $OUT/callgraph.json $OUT/syscalls_with_callsites.txt $OUT/startfuncs_with_addr.txt > $OUT/syscalls.txt
fi
