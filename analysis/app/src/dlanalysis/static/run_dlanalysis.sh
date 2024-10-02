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

$script_dir/../../../syspart -p ${BINARY} -i -s main -a 6,dlopen@@GLIBC_2.2.5,7 > $DLOUT/dlopen_static.txt && echo OK

$script_dir/../../../syspart -p ${BINARY} -i -s main -a 6,dlsym,6 > $DLOUT/dlsym_static.txt && echo OK

$script_dir/generate_libnames.sh ${BINARY}
        
$script_dir/match_libs_with_syms.sh $DLOUT/dlsym_static.txt ${BINARY_NAME} > $DLOUT/libraries_matching_syms.txt && echo OK
        
$script_dir/../../../syspart -p ${BINARY} -i -s main -a 8,$DLOUT/libraries_matching_syms.txt && echo OK
