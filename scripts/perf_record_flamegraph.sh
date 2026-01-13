#!/bin/bash
# Record CPU samples and generate flamegraph for event processor
#
# Requires:
# - Linux with perf tools
# - FlameGraph tools in PATH (https://github.com/brendangregg/FlameGraph)
#
# Usage: ./scripts/perf_record_flamegraph.sh [duration_seconds]

set -e

# Check if running on Linux
if [[ "$(uname)" != "Linux" ]]; then
    echo "ERROR: perf requires Linux kernel"
    echo "Current OS: $(uname)"
    echo ""
    echo "This script uses Linux perf for performance profiling."
    echo "On macOS, consider using Instruments instead."
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

# Check if FlameGraph tools are in PATH
if ! command -v stackcollapse-perf.pl &> /dev/null || ! command -v flamegraph.pl &> /dev/null; then
    echo "ERROR: FlameGraph tools not found in PATH"
    echo ""
    echo "Install FlameGraph:"
    echo "  git clone https://github.com/brendangregg/FlameGraph"
    echo "  export PATH=\$PATH:\$(pwd)/FlameGraph"
    echo ""
    echo "Or add to ~/.bashrc:"
    echo "  export PATH=\$PATH:/path/to/FlameGraph"
    exit 1
fi

DURATION=${1:-30}
PERF_DATA="perf.data"
FLAMEGRAPH_SVG="flamegraph.svg"
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

echo "=== FlameGraph Generation ==="
echo "Duration: ${DURATION} seconds"
echo "Output: ${FLAMEGRAPH_SVG}"
echo ""

# Record perf data
echo "Recording perf data for ${DURATION} seconds..."
echo "Sampling at 99 Hz with DWARF call graphs..."
perf record -F 99 -g --call-graph=dwarf -o "$PERF_DATA" \
            timeout "${DURATION}s" ./event_processor "$EVENT_LOG" || true

echo ""
echo "Generating flamegraph..."

# Generate flamegraph
perf script -i "$PERF_DATA" | \
    stackcollapse-perf.pl | \
    flamegraph.pl > "$FLAMEGRAPH_SVG"

echo ""
echo "=== FlameGraph Complete ==="
echo "Flamegraph saved to: $FLAMEGRAPH_SVG"
echo "Size: $(du -h "$FLAMEGRAPH_SVG" | cut -f1)"
echo ""
echo "Open in browser:"
echo "  xdg-open $FLAMEGRAPH_SVG"
echo "  or"
echo "  firefox $FLAMEGRAPH_SVG"
echo ""
echo "Expected hotspots:"
echo "  - EventLogReader::readNext() should be 50%+ of CPU time"
echo "  - EventParser::parse() should be 20-30%"
echo "  - Unexpected allocations (>10%) may indicate optimization opportunities"
