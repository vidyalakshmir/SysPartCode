#!/bin/bash
echo "Running helloIndirect/test.sh"

LD_LIBRARY_PATH=hello ../analysis/app/syspart -p helloIndirect/main -s main -i -a 1 > helloIndirect/hello.out

grep -q '^DIRECT.*executable.*printHello.*module-libhello.so' helloIndirect/hello.out
direct_printHello_link=$?

grep -q '^DIRECT.*libhello.so.*printHello.*module-libstdc++.so.6' helloIndirect/hello.out
libstdc_link=$?

if [ $direct_printHello_link -ne 0 ] && [ $libstdc_link -eq 0 ]; then
    echo "Test passed: Indirect call to printHello confirmed; direct link absent, and library linked to libstdc++."
    exit 0
else
    echo "Test failed: Direct link to printHello exists, or library linkage to libstdc++ missing."
    exit 1
fi

