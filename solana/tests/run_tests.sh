#!/bin/bash
# PikaPython Test Suite
#
# Usage:
#   ./run_tests.sh           Run all tests
#   ./run_tests.sh vm        Run VM-only tests (bytecode)
#   ./run_tests.sh parser    Run parser+VM tests (source)
#   ./run_tests.sh cpi       Run CPI tests
#   ./run_tests.sh vfs       Run VFS tests
#   ./run_tests.sh import    Run import/exec tests
#   ./run_tests.sh errors    Run error handling tests
#   ./run_tests.sh -v        Run all with verbose output

set -e
cd "$(dirname "$0")"

VERBOSE=""
if [[ "$*" == *"-v"* ]] || [[ "$*" == *"--verbose"* ]]; then
    VERBOSE="--verbose"
fi

echo "========================================"
echo "PikaPython Test Suite"
echo "========================================"
echo ""

run_test() {
    local name=$1
    local file=$2
    echo "Running $name..."
    if node "$file" $VERBOSE; then
        echo ""
    else
        echo ""
        echo "FAILED: $name"
        exit 1
    fi
}

case "$1" in
    vm)
        run_test "VM-only tests" "test_vm_only.js"
        ;;
    parser)
        run_test "Parser+VM tests" "test_parser_vm.js"
        ;;
    cpi)
        run_test "CPI tests" "test_cpi.js"
        ;;
    vfs)
        run_test "VFS tests" "test_vfs.js"
        ;;
    import)
        run_test "Import/exec tests" "test_import_module.js"
        ;;
    errors)
        run_test "Error handling tests" "test_errors.js"
        ;;
    *)
        # Run all tests
        run_test "VM-only tests (bytecode)" "test_vm_only.js"
        run_test "Parser+VM tests (source)" "test_parser_vm.js"
        run_test "CPI tests" "test_cpi.js"
        run_test "VFS tests" "test_vfs.js"
        run_test "Import/exec tests" "test_import_module.js"
        run_test "Error handling tests" "test_errors.js"

        echo "========================================"
        echo "All tests passed!"
        echo "========================================"
        ;;
esac
