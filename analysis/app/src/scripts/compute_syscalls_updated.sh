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
./syspart -p $APP -s $STARTFILE -i -a 27 > $OUT/syscalls.txt
