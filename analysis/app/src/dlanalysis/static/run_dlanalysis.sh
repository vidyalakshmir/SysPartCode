#!/bin/bash
#src/dlanalysis/static/run_dlanalysis.sh src/dlanalysis/binaries        

#BIN_PATH=$1
script_dir=$(dirname "$(realpath "$0")")
cd $script_dir/../../../
make
cd src/dlanalysis/static
BINARY=$1
DLOUT=$2        
echo "Dlanalysis for ${BINARY_NAME}"

dlopen=$(cat $script_dir/../dlopen.txt)
dlsym=$(cat $script_dir/../dlsym.txt)

$script_dir/../../../syspart -p ${BINARY} -i -s main -a 6,$dlopen,7 > $DLOUT/dlopen_static.txt && echo OK

$script_dir/../../../syspart -p ${BINARY} -i -s main -a 6,$dlsym,6 > $DLOUT/dlsym_static.txt && echo OK

python3 process_result.py $DLOUT/dlsym_static.txt $dlopen > $DLOUT/dlopen_args.txt && echo OK

python3 process_result.py $DLOUT/dlsym_static.txt $dlsym > $DLOUT/dlsym_args.txt && echo OK

$script_dir/generate_libnames.sh ${BINARY}
        
$script_dir/match_libs_with_syms.sh $DLOUT/dlsym_static.txt > $DLOUT/libraries_matching_syms.txt && echo OK
