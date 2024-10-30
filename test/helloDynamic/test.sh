#!/bin/bash
echo "Running dynamic analysis on helloDynamic/main "

# Define paths
MAIN_EXECUTABLE="$PWD/helloDynamic/main"
OUTPUT_DIR="$PWD/helloDynamic"
ANALYSIS_DIR="../analysis/app/src/dlanalysis/static"

# Change to the analysis directory and run the analysis script
(cd $ANALYSIS_DIR && ./run_dlanalysis.sh $MAIN_EXECUTABLE $OUTPUT_DIR)

# Check for printHello in the output
grep -q 'SYM printHello' helloDynamic/dlsym_static.txt
printHello_check=$?

# Check for libhello.so in the output
grep -q 'SYM hello/libhello.so' helloDynamic/dlopen_static.txt
libhello_check=$?

# Evaluate the results
if [ $printHello_check -eq 0 ] && [ $libhello_check -eq 0 ]; then
    echo "Test passed: Successfully found printHello in dlsym_static.txt and libhello.so in dlopen_static.txt."
    exit 0
else
    echo "Test failed: Expected entries for printHello or libhello.so not found in output."
    exit 1
fi

