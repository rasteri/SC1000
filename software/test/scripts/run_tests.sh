#!/bin/bash
#
# SC1000 Automated Test Runner
#
# Builds and runs audio engine tests, exports WAV files,
# and optionally runs Python spectral analysis.
#
# Usage:
#   ./test/scripts/run_tests.sh              # Run tests only
#   ./test/scripts/run_tests.sh --analyze    # Run tests + Python analysis
#   ./test/scripts/run_tests.sh --clean      # Clean build first
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_DIR="$(dirname "$TEST_DIR")"
BUILD_DIR="$PROJECT_DIR/cmake-build-debug"
OUTPUT_DIR="$TEST_DIR/output"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Parse arguments
DO_ANALYZE=false
DO_CLEAN=false
DO_BUILD=true

for arg in "$@"; do
    case $arg in
        --analyze|-a)
            DO_ANALYZE=true
            ;;
        --clean|-c)
            DO_CLEAN=true
            ;;
        --no-build)
            DO_BUILD=false
            ;;
        --help|-h)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --analyze, -a    Run Python spectral analysis after tests"
            echo "  --clean, -c      Clean build directory first"
            echo "  --no-build       Skip build step (use existing binary)"
            echo "  --help, -h       Show this help"
            exit 0
            ;;
    esac
done

cd "$PROJECT_DIR"

# Clean if requested
if $DO_CLEAN; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
fi

# Build
if $DO_BUILD; then
    echo -e "${YELLOW}Configuring CMake...${NC}"
    cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON -DNATIVE=ON > /dev/null

    echo -e "${YELLOW}Building sc1000-test...${NC}"
    cmake --build "$BUILD_DIR" --target sc1000-test -- -j$(nproc) 2>&1 | tail -5
fi

# Check binary exists
if [ ! -f "$BUILD_DIR/sc1000-test" ]; then
    echo -e "${RED}Error: sc1000-test not found. Run with --clean to rebuild.${NC}"
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Run tests
echo ""
echo -e "${YELLOW}Running tests...${NC}"
echo "========================================"

if "$BUILD_DIR/sc1000-test" --dump "$OUTPUT_DIR" 2>&1 | grep -v "^/home\|^loop_buffer\|allocated"; then
    TEST_RESULT=0
else
    TEST_RESULT=$?
fi

echo "========================================"

# Count WAV files
WAV_COUNT=$(ls -1 "$OUTPUT_DIR"/*.wav 2>/dev/null | wc -l)
echo -e "Generated ${GREEN}$WAV_COUNT${NC} WAV files in $OUTPUT_DIR/"

# Run Python analysis if requested
if $DO_ANALYZE; then
    echo ""
    echo -e "${YELLOW}Running Python analysis...${NC}"

    # Check for Python and dependencies
    if ! command -v python3 &> /dev/null; then
        echo -e "${RED}Error: python3 not found${NC}"
        exit 1
    fi

    # Check for required modules
    if ! python3 -c "import numpy, scipy, matplotlib" 2>/dev/null; then
        echo -e "${RED}Error: Missing Python dependencies${NC}"
        echo "Install with: pip install numpy scipy matplotlib librosa"
        exit 1
    fi

    # Run analysis
    python3 "$SCRIPT_DIR/analyze_test_output.py" \
        --output "$OUTPUT_DIR" \
        --no-show \
        "$OUTPUT_DIR"/*.wav

    echo ""
    echo -e "Analysis plots saved to ${GREEN}$OUTPUT_DIR/${NC}"

    # List generated files
    ls -1 "$OUTPUT_DIR"/*.png 2>/dev/null | while read f; do
        echo "  $(basename "$f")"
    done
fi

echo ""
if [ $TEST_RESULT -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
else
    echo -e "${RED}Some tests failed.${NC}"
fi

exit $TEST_RESULT
