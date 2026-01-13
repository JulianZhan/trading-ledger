#pragma once

#include <string>
#include <chrono>

#ifdef __linux__
#include <sys/inotify.h>
#endif

namespace trading_ledger {

/**
 * Event log tail-follower
 *
 * Waits for file modifications efficiently:
 * - Linux: Uses inotify for <100µs latency
 * - Other platforms: Uses polling with exponential backoff
 */
class EventLogTailer {
public:
    explicit EventLogTailer(const std::string& log_path);
    ~EventLogTailer();

    // Non-copyable
    EventLogTailer(const EventLogTailer&) = delete;
    EventLogTailer& operator=(const EventLogTailer&) = delete;

    /**
     * Initialize the tailer
     * Throws std::runtime_error on failure
     */
    void init();

    /**
     * Wait for file to be modified
     * Blocks until new data available (or timeout)
     *
     * @param timeout_ms Max milliseconds to wait (0 = infinite)
     * @return true if file was modified, false on timeout
     */
    bool waitForModification(int timeout_ms = 0);

    /**
     * Check if using inotify (Linux) or polling (fallback)
     */
    bool isUsingInotify() const {
#ifdef __linux__
        return true;
#else
        return false;
#endif
    }

private:
    std::string log_path_;

#ifdef __linux__
    // Linux: inotify-based
    int inotify_fd_;
    int watch_fd_;
#else
    // Fallback: polling-based
    size_t last_known_size_;
    int poll_interval_ms_;
    static constexpr int MIN_POLL_INTERVAL_MS = 10;
    static constexpr int MAX_POLL_INTERVAL_MS = 100;
#endif

    // Helper: get current file size
    size_t getFileSize() const;
};

}  // namespace trading_ledger
