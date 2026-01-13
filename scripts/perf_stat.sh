#!/bin/bash
# Measure CPU efficiency of event processor using Linux perf
#
# Requires: Linux with perf tools installed
# Usage: ./scripts/perf_stat.sh [duration_seconds]

set -e

# Check if running on Linux
if [[ "$(uname)" != "Linux" ]]; then
    echo "ERROR: perf requires Linux kernel"
    echo "Current OS: $(uname)"
    echo ""
    echo "This script uses Linux perf for performance measurement."
    echo "On macOS, consider using Instruments or DTrace instead."
    exit 1
fi

# Check if perf is installed
if ! command -v perf &> /dev/null; then
    echo "ERROR: perf not found"
    echo ""
    echo "Install perf tools:"
    echo "  Ubuntu/Debian: sudo apt-get install linux-tools-common linux-tools-generic"
    echo "  Fedora/RHEL:   sudo dnf install perf"
    exit 1
fi

DURATION=${1:-60}
OUTPUT_FILE="perf_stat_output.txt"
EVENT_LOG="../data/event_log.bin"

# Check if event_processor exists
if [[ ! -f "./event_processor" ]]; then
    echo "ERROR: event_processor not found in current directory"
    echo "Expected: ./event_processor"
    echo ""
    echo "Build first:"
    echo "  cd cpp"
    echo "  cmake --build build"
    echo "  cd build"
    exit 1
fi

echo "=== Linux perf stat measurement ==="
echo "Duration: ${DURATION} seconds"
echo "Output: ${OUTPUT_FILE}"
echo ""

# Run event processor with perf stat
echo "Starting event processor with perf measurement..."
perf stat -e cycles,instructions,cache-references,cache-misses,branches,branch-misses \
          -o "$OUTPUT_FILE" \
          timeout "${DURATION}s" ./event_processor "$EVENT_LOG" || true

echo ""
echo "=== Performance Counter Summary ==="
cat "$OUTPUT_FILE"

echo ""
echo "=== Target Metrics ==="
echo "- IPC (Instructions Per Cycle): > 2.0"
echo "- Cache miss rate: < 10%"
echo "- Branch miss rate: < 1%"
echo ""
echo "Analysis saved to: $OUTPUT_FILE"
