#pragma once

#include <map>
#include <cstdint>
#include <vector>
#include <ostream>
#include <iostream>

namespace trading_ledger {

/**
 * Histogram for tracking latency measurements and calculating percentiles.
 *
 * Stores latencies in nanoseconds and provides efficient percentile calculations.
 * Designed for measuring end-to-end latency from Java event write to C++ processing.
 */
class LatencyHistogram {
public:
    LatencyHistogram() = default;

    /**
     * Record a latency measurement in nanoseconds
     */
    void record(int64_t latency_ns);

    /**
     * Get the total number of samples recorded
     */
    size_t count() const { return total_count_; }

    /**
     * Calculate percentile value (0.0 to 1.0, e.g., 0.99 for p99)
     * Returns latency in nanoseconds
     */
    int64_t percentile(double p) const;

    /**
     * Get minimum latency in nanoseconds
     */
    int64_t min() const;

    /**
     * Get maximum latency in nanoseconds
     */
    int64_t max() const;

    /**
     * Get mean (average) latency in nanoseconds
     */
    int64_t mean() const;

    /**
     * Print summary statistics to output stream
     */
    void printSummary(std::ostream& out = std::cout) const;

    /**
     * Clear all recorded samples
     */
    void clear();

private:
    // Map from latency (ns) -> count
    std::map<int64_t, size_t> samples_;
    size_t total_count_ = 0;
    int64_t sum_ = 0;  // For mean calculation
};

} // namespace trading_ledger
