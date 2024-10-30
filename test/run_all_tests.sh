#!/bin/bash

tests=$(find . -type f -name "test.sh")
passed=0
failed=0

for test in $tests; do
    if [ -x "$test" ]; then
        echo "================================="
        output=$($test)
        echo "$output"
        if echo "$output" | grep -q "Test passed"; then
            ((passed++))
        else
            ((failed++))
        fi
    else
        ((failed++))
    fi
done

echo "================================="
echo "Test Summary: $passed passed, $failed failed."
echo "================================="

if [ $failed -eq 0 ]; then
    exit 0
else
    exit 1
fi

