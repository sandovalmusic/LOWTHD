#!/bin/bash

# LOW THD TAPE SIMULATOR v1.0 - Master Test Suite
# Runs all validation tests and reports overall status

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$SCRIPT_DIR/build"

echo "╔══════════════════════════════════════════════════════════════════╗"
echo "║        LOW THD TAPE SIMULATOR v1.0 - MASTER TEST SUITE           ║"
echo "╚══════════════════════════════════════════════════════════════════╝"
echo ""

# Create build directory
mkdir -p "$BUILD_DIR"

# List of all tests
TESTS=(
    "Test_THDAccuracy"
    "Test_HarmonicBalance"
    "Test_FrequencyResponse"
    "Test_Transparency"
    "Test_PhaseCoherence"
    "Test_Stereo"
    "Test_Stability"
)

PASSED=0
FAILED=0
FAILED_TESTS=""

# Compile and run each test
for TEST in "${TESTS[@]}"; do
    echo ""
    echo "════════════════════════════════════════════════════════════════════"
    echo "  Compiling: $TEST"
    echo "════════════════════════════════════════════════════════════════════"

    # Compile
    if clang++ -std=c++17 -O2 -I"$PROJECT_DIR" \
        "$SCRIPT_DIR/$TEST.cpp" \
        -o "$BUILD_DIR/$TEST" 2>&1; then

        echo "  Running: $TEST"
        echo "────────────────────────────────────────────────────────────────────"

        # Run test
        if "$BUILD_DIR/$TEST"; then
            PASSED=$((PASSED + 1))
        else
            FAILED=$((FAILED + 1))
            FAILED_TESTS="$FAILED_TESTS\n  - $TEST"
        fi
    else
        echo "  COMPILE ERROR: $TEST"
        FAILED=$((FAILED + 1))
        FAILED_TESTS="$FAILED_TESTS\n  - $TEST (compile error)"
    fi
done

# Summary
echo ""
echo "╔══════════════════════════════════════════════════════════════════╗"
echo "║                        TEST SUMMARY                              ║"
echo "╚══════════════════════════════════════════════════════════════════╝"
echo ""
echo "  Total Tests:  $((PASSED + FAILED))"
echo "  Passed:       $PASSED"
echo "  Failed:       $FAILED"

if [ $FAILED -gt 0 ]; then
    echo ""
    echo "  Failed tests:"
    echo -e "$FAILED_TESTS"
    echo ""
    echo "════════════════════════════════════════════════════════════════════"
    echo "  RESULT: SOME TESTS FAILED"
    echo "════════════════════════════════════════════════════════════════════"
    exit 1
else
    echo ""
    echo "════════════════════════════════════════════════════════════════════"
    echo "  RESULT: ALL TESTS PASSED - PLUGIN IS SPOTLESS!"
    echo "════════════════════════════════════════════════════════════════════"
    exit 0
fi
