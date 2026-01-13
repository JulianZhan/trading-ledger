#include "LatencyHistogram.h"
#include <algorithm>
#include <iomanip>

namespace trading_ledger {

void LatencyHistogram::record(int64_t latency_ns) {
    samples_[latency_ns]++;
    total_count_++;
    sum_ += latency_ns;
}

int64_t LatencyHistogram::percentile(double p) const {
    if (total_count_ == 0) {
        return 0;
    }

    // Calculate target index (0-based)
    size_t target_index = static_cast<size_t>(p * total_count_);
    if (target_index >= total_count_) {
        target_index = total_count_ - 1;
    }

    // Iterate through sorted samples to find percentile
    size_t cumulative = 0;
    for (const auto& [latency, count] : samples_) {
        cumulative += count;
        if (cumulative > target_index) {
            return latency;
        }
    }

    // Should not reach here
    return max();
}

int64_t LatencyHistogram::min() const {
    if (samples_.empty()) {
        return 0;
    }
    return samples_.begin()->first;
}

int64_t LatencyHistogram::max() const {
    if (samples_.empty()) {
        return 0;
    }
    return samples_.rbegin()->first;
}

int64_t LatencyHistogram::mean() const {
    if (total_count_ == 0) {
        return 0;
    }
    return sum_ / static_cast<int64_t>(total_count_);
}

void LatencyHistogram::printSummary(std::ostream& out) const {
    if (total_count_ == 0) {
        out << "No latency samples recorded" << std::endl;
        return;
    }

    out << "\n=== Latency Summary (n=" << total_count_ << ") ===" << std::endl;
    out << std::fixed << std::setprecision(2);

    // Convert to microseconds for readability
    out << "  Min:  " << std::setw(8) << (min() / 1000.0) << " µs" << std::endl;
    out << "  Mean: " << std::setw(8) << (mean() / 1000.0) << " µs" << std::endl;
    out << "  p50:  " << std::setw(8) << (percentile(0.50) / 1000.0) << " µs" << std::endl;
    out << "  p90:  " << std::setw(8) << (percentile(0.90) / 1000.0) << " µs" << std::endl;
    out << "  p99:  " << std::setw(8) << (percentile(0.99) / 1000.0) << " µs" << std::endl;
    out << "  p999: " << std::setw(8) << (percentile(0.999) / 1000.0) << " µs" << std::endl;
    out << "  Max:  " << std::setw(8) << (max() / 1000.0) << " µs" << std::endl;

    // Acceptance criteria check
    int64_t p99_ns = percentile(0.99);
    int64_t p999_ns = percentile(0.999);

    out << "\n=== Acceptance Criteria ===" << std::endl;
    out << "  p99 < 200µs:  " << (p99_ns < 200000 ? "PASS" : "FAIL")
        << " (actual: " << (p99_ns / 1000.0) << "µs)" << std::endl;
    out << "  p999 < 500µs: " << (p999_ns < 500000 ? "PASS" : "FAIL")
        << " (actual: " << (p999_ns / 1000.0) << "µs)" << std::endl;
}

void LatencyHistogram::clear() {
    samples_.clear();
    total_count_ = 0;
    sum_ = 0;
}

} // namespace trading_ledger
