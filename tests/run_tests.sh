#!/bin/bash
# Test runner for the C subset compiler

COMPILER="../build/mycc"
TESTS_DIR="."
PASS=0
FAIL=0

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Function to run a test
run_test() {
    local test_name=$1
    local source_file="${test_name}.c"
    local expected_output="${test_name}.expected"
    
    echo -n "Running test: ${test_name} ... "
    
    # Compile with our compiler
    ${COMPILER} -o ${test_name}_mycc ${source_file} 2>/dev/null
    if [ $? -ne 0 ]; then
        echo -e "${RED}FAIL${NC} (compilation failed with mycc)"
        FAIL=$((FAIL + 1))
        return
    fi
    
    # Compile with GCC
    gcc -o ${test_name}_gcc ${source_file} 2>/dev/null
    if [ $? -ne 0 ]; then
        echo -e "${RED}FAIL${NC} (compilation failed with gcc)"
        FAIL=$((FAIL + 1))
        return
    fi
    
    # Run both and compare output
    ./${test_name}_mycc > ${test_name}_mycc.out
    mycc_exit=$?
    ./${test_name}_gcc > ${test_name}_gcc.out
    gcc_exit=$?
    
    # Compare outputs
    if diff -q ${test_name}_mycc.out ${test_name}_gcc.out > /dev/null && [ ${mycc_exit} -eq ${gcc_exit} ]; then
        echo -e "${GREEN}PASS${NC}"
        PASS=$((PASS + 1))
    else
        echo -e "${RED}FAIL${NC} (output mismatch)"
        echo "  mycc output:"
        cat ${test_name}_mycc.out | head -5
        echo "  gcc output:"
        cat ${test_name}_gcc.out | head -5
        FAIL=$((FAIL + 1))
    fi
    
    # Cleanup
    rm -f ${test_name}_mycc ${test_name}_gcc ${test_name}_mycc.out ${test_name}_gcc.out
}

# Find all test files
cd ${TESTS_DIR}
for test_file in test_*.c; do
    if [ -f "${test_file}" ]; then
        test_name="${test_file%.c}"
        run_test ${test_name}
    fi
done

# Summary
echo ""
echo "================================"
echo "Test Results:"
echo "  Passed: ${PASS}"
echo "  Failed: ${FAIL}"
echo "================================"

if [ ${FAIL} -eq 0 ]; then
    exit 0
else
    exit 1
fi
