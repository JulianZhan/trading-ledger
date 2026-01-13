#!/bin/bash
# End-to-end latency benchmark
#
# Starts PostgreSQL, Java API, C++ processor, and runs load test
# Measures Java -> C++ latency distribution
#
# Usage: ./scripts/bench_end_to_end.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=== End-to-End Latency Benchmark ==="
echo "Project root: $PROJECT_ROOT"
echo ""

# Clean up function
cleanup() {
    echo ""
    echo "Cleaning up..."
    if [[ -n "$JAVA_PID" ]]; then
        kill $JAVA_PID 2>/dev/null || true
    fi
    if [[ -n "$CPP_PID" ]]; then
        kill $CPP_PID 2>/dev/null || true
    fi
    cd "$PROJECT_ROOT"
    docker compose down 2>/dev/null || true
}

trap cleanup EXIT

# Step 1: Start PostgreSQL
echo "Step 1: Starting PostgreSQL..."
cd "$PROJECT_ROOT"
docker compose up -d postgres
sleep 5

# Check if PostgreSQL is ready
echo "Waiting for PostgreSQL to be ready..."
for i in {1..30}; do
    if docker compose exec -T postgres pg_isready -U ledger_user &>/dev/null; then
        echo "PostgreSQL is ready"
        break
    fi
    if [[ $i -eq 30 ]]; then
        echo "ERROR: PostgreSQL failed to start"
        exit 1
    fi
    sleep 1
done

# Step 2: Start Java API
echo ""
echo "Step 2: Starting Java API..."
cd "$PROJECT_ROOT/java"
mvn spring-boot:run > /tmp/java.log 2>&1 &
JAVA_PID=$!
echo "Java PID: $JAVA_PID"

# Wait for Spring Boot to start
echo "Waiting for Spring Boot to start..."
sleep 15

# Check if Java API is responding
for i in {1..30}; do
    if curl -s http://localhost:8080/actuator/health &>/dev/null; then
        echo "Java API is ready"
        break
    fi
    if [[ $i -eq 30 ]]; then
        echo "ERROR: Java API failed to start"
        echo "Check logs: tail /tmp/java.log"
        exit 1
    fi
    sleep 1
done

# Step 3: Start C++ processor
echo ""
echo "Step 3: Starting C++ event processor..."
cd "$PROJECT_ROOT/cpp/build"

if [[ ! -f ./event_processor ]]; then
    echo "ERROR: event_processor not found"
    echo "Build first: cd cpp && cmake --build build"
    exit 1
fi

# Note: In Day 9 MVP, we'll capture latency metrics in future enhancement
# For now, just run the processor
./event_processor ../data/event_log.bin > /tmp/cpp_metrics.txt 2>&1 &
CPP_PID=$!
echo "C++ PID: $CPP_PID"
sleep 2

# Step 4: Run load generator
echo ""
echo "Step 4: Running load generator (10K trades/sec for 60 sec)..."
cd "$PROJECT_ROOT/java"

# Compile LoadGenerator if not already compiled
mvn compile -q

# Run LoadGenerator
mvn exec:java -Dexec.mainClass=com.trading.ledger.LoadGenerator \
              -Dexec.args="--rate=10000 --duration=60" \
              -q

echo ""
echo "=== Load Test Complete ==="

# Step 5: Check results
echo ""
echo "Waiting for C++ processor to catch up (5 seconds)..."
sleep 5

# Stop C++ processor gracefully
echo "Stopping C++ processor..."
kill -INT $CPP_PID 2>/dev/null || true
wait $CPP_PID 2>/dev/null || true

# Display results
echo ""
echo "=== C++ Processing Statistics ==="
tail -20 /tmp/cpp_metrics.txt

echo ""
echo "=== Benchmark Complete ==="
echo ""
echo "Results:"
echo "  Java logs:  /tmp/java.log"
echo "  C++ output: /tmp/cpp_metrics.txt"
echo ""
echo "Next steps (Day 9 full implementation):"
echo "  - Add latency measurement (Java write timestamp -> C++ process timestamp)"
echo "  - Calculate p50/p90/p99/p999 latencies"
echo "  - Target: p99 < 200µs on Linux with inotify"
echo ""
echo "Current verification:"
echo "  - Check that C++ processed all events (see counts in /tmp/cpp_metrics.txt)"
echo "  - Verify no validation errors"
