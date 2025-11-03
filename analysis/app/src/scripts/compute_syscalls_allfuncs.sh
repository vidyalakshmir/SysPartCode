show_help() {
    cat << EOF
Usage: $0 arg1 arg2 [--log]

Arguments:
  arg1    ELF binary to be analysed
  arg2    Folder to store outputs

Options:
  --log   This will log the paths to the system calls from the start functions in logfile.txt in the output folder
  -h, --help
          Show this help message and exit

Outputs:
  syscalls.txt			List of system calls reachable from the start functions
EOF
}

if [[ $# -lt 2 || "$1" == "-h" || "$1" == "--help" ]]; then
    show_help
    exit 0
fi

APP=$1
OUT=$(realpath $2)
shift 2

log_flag=false
while [[ $# -gt 0 ]]; do
    case $1 in
        --log) log_flag=true ;;
    esac
    shift
done

./syspart -p $APP -s all -i -a 28 > $OUT/syscalls.txt
