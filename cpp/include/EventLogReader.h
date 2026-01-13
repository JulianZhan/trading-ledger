#pragma once

#include "Event.h"
#include "EventParser.h"
#include <string>
#include <cstdint>

namespace trading_ledger {

/**
 * Memory-mapped event log reader
 *
 * Reads events from append-only binary log file using mmap for efficient I/O.
 * On Linux, uses madvise(MADV_SEQUENTIAL) for optimal readahead.
 */
class EventLogReader {
public:
    explicit EventLogReader(const std::string& log_path);
    ~EventLogReader();

    // Non-copyable
    EventLogReader(const EventLogReader&) = delete;
    EventLogReader& operator=(const EventLogReader&) = delete;

    /**
     * Open log file and map to memory
     * Throws std::runtime_error on failure
     */
    void open();

    /**
     * Read next event from log
     * @param event Output parameter to store event
     * @return true if event read, false if EOF
     * @throws ParseException on corrupted data
     */
    bool readNext(Event& event);

    /**
     * Get current file offset (bytes from start)
     */
    size_t offset() const { return offset_; }

    /**
     * Get total file size
     */
    size_t fileSize() const { return file_size_; }

    /**
     * Check if at end of file
     */
    bool eof() const { return offset_ >= file_size_; }

    /**
     * Remap file if it has grown (for tail-following)
     * @return true if file was remapped, false if unchanged
     */
    bool remapIfGrown();

private:
    std::string log_path_;
    int fd_;                    // File descriptor
    uint8_t* mapped_data_;      // Memory-mapped file data
    size_t file_size_;          // Current mapped size
    size_t offset_;             // Current read offset
    FileHeader file_header_;    // Cached file header
    bool is_open_;
};

}  // namespace trading_ledger
