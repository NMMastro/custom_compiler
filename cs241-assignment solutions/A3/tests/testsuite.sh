#!/bin/bash

# Change to the script's directory
cd "$(dirname "$0")" || exit 1

# Initialize counters
passed=0
failed=0
failed_tests=()

# Read test names from suite.txt
if [ ! -f suite.txt ]; then
    echo "Error: suite.txt not found"
    exit 1
fi

# Process each test from suite.txt
while IFS= read -r test || [ -n "$test" ]; do
    # Skip empty lines and comments
    [[ -z "$test" || "$test" =~ ^# ]] && continue
    
    echo "Running test: $test"
    
    # Step 1: Tokenize the input
    ./tokenize < "${test}.asm" > "${test}.tokenized"
    
    # Step 2: Run the assembler
    ../asm < "${test}.tokenized" > "${test}.result"
    
    # Step 3: Generate expected output
    cs241.binasm < "${test}.asm" > "${test}.expect"
    
    # Step 4: Compare using binview
    expected=$(cs241.binview --all "${test}.expect")
    received=$(cs241.binview --all "${test}.result")
    
    if [ "$expected" = "$received" ]; then
        echo "✓ PASSED: $test"
        # Cleanup for passing tests
        rm -f "${test}.tokenized" "${test}.result" "${test}.expect"
        passed=$((passed + 1))
    else
        echo "✗ FAILED: $test"
        echo "EXPECTED:"
        echo "$expected"
        echo "________________________________"
        echo "RECEIVED:"
        echo "$received"
        failed=$((failed + 1))
        failed_tests+=("$test")
        # Stop on first failure
        break
    fi
done < suite.txt

# Print summary
echo ""
echo "========== TEST SUMMARY =========="
echo "Passed: $passed"
echo "Failed: $failed"
if [ $failed -gt 0 ]; then
    echo "Failed tests: ${failed_tests[@]}"
    exit 1
else
    echo "All tests passed!"
    exit 0
fi