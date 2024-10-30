#!/bin/bash

# Run the syspart analysis
LD_LIBRARY_PATH=hello ../analysis/app/syspart -p hello/main -s main -i -a 1 > hello/hello.out

# Check for direct link from main to printHello in the library
grep -q '^DIRECT.*executable.*printHello.*module-libhello.so' hello/hello.out
direct_printHello_link=$?

# Check for library link to libstdc++
grep -q '^DIRECT.*libhello.so.*printHello.*module-libstdc++.so.6' hello/hello.out
libstdc_link=$?

# Evaluate the results
if [ $direct_printHello_link -eq 0 ] && [ $libstdc_link -eq 0 ]; then
    echo "Test passed: Direct link of printHello to library and library to libstdc++ confirmed."
    exit 0
else
    echo "Test failed: Expected links not found in output."
    exit 1
fi

