#include "EventLogTailer.h"
#include <stdexcept>
#include <thread>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/select.h>
#endif

namespace trading_ledger {

EventLogTailer::EventLogTailer(const std::string& log_path)
    : log_path_(log_path)
#ifdef __linux__
    , inotify_fd_(-1)
    , watch_fd_(-1)
#else
    , last_known_size_(0)
    , poll_interval_ms_(MIN_POLL_INTERVAL_MS)
#endif
{}

EventLogTailer::~EventLogTailer() {
#ifdef __linux__
    if (watch_fd_ >= 0) {
        inotify_rm_watch(inotify_fd_, watch_fd_);
    }
    if (inotify_fd_ >= 0) {
        close(inotify_fd_);
    }
#endif
}

void EventLogTailer::init() {
#ifdef __linux__
    // Linux: Initialize inotify
    inotify_fd_ = inotify_init1(IN_NONBLOCK);
    if (inotify_fd_ < 0) {
        throw std::runtime_error("Failed to initialize inotify");
    }

    // Watch for modifications and close-write events
    watch_fd_ = inotify_add_watch(inotify_fd_, log_path_.c_str(),
                                   IN_MODIFY | IN_CLOSE_WRITE);
    if (watch_fd_ < 0) {
        close(inotify_fd_);
        throw std::runtime_error("Failed to add inotify watch for: " + log_path_);
    }
#else
    // Non-Linux: Initialize polling state
    last_known_size_ = getFileSize();
    poll_interval_ms_ = MIN_POLL_INTERVAL_MS;
#endif
}

bool EventLogTailer::waitForModification(int timeout_ms) {
#ifdef __linux__
    // Linux: Use inotify with select for efficient blocking
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(inotify_fd_, &readfds);

    struct timeval timeout;
    struct timeval* timeout_ptr = nullptr;

    if (timeout_ms > 0) {
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        timeout_ptr = &timeout;
    }

    // Block until inotify event or timeout
    int ret = select(inotify_fd_ + 1, &readfds, nullptr, nullptr, timeout_ptr);

    if (ret < 0) {
        throw std::runtime_error("select() failed on inotify fd");
    }

    if (ret == 0) {
        return false;  // Timeout
    }

    // Drain inotify event queue
    char buffer[4096];
    ssize_t len = read(inotify_fd_, buffer, sizeof(buffer));
    if (len < 0 && errno != EAGAIN) {
        throw std::runtime_error("Failed to read inotify events");
    }

    return true;  // File was modified

#else
    // Non-Linux: Polling with exponential backoff
    auto start = std::chrono::steady_clock::now();

    while (true) {
        size_t current_size = getFileSize();

        // Check if file has grown
        if (current_size > last_known_size_) {
            last_known_size_ = current_size;
            poll_interval_ms_ = MIN_POLL_INTERVAL_MS;  // Reset backoff
            return true;
        }

        // Check timeout
        if (timeout_ms > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start
            ).count();

            if (elapsed >= timeout_ms) {
                return false;  // Timeout
            }
        }

        // Sleep with exponential backoff
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms_));

        // Increase poll interval (exponential backoff)
        poll_interval_ms_ = std::min(poll_interval_ms_ * 2, MAX_POLL_INTERVAL_MS);
    }
#endif
}

size_t EventLogTailer::getFileSize() const {
    struct stat st;
    if (stat(log_path_.c_str(), &st) != 0) {
        return 0;
    }
    return st.st_size;
}

}  // namespace trading_ledger
