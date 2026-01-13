#include "EventLogReader.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdexcept>
#include <cstring>

namespace trading_ledger {

EventLogReader::EventLogReader(const std::string& log_path)
    : log_path_(log_path)
    , fd_(-1)
    , mapped_data_(nullptr)
    , file_size_(0)
    , offset_(0)
    , is_open_(false) {}

EventLogReader::~EventLogReader() {
    if (mapped_data_ != nullptr && mapped_data_ != MAP_FAILED) {
        munmap(mapped_data_, file_size_);
    }
    if (fd_ >= 0) {
        close(fd_);
    }
}

void EventLogReader::open() {
    // Open file read-only
    fd_ = ::open(log_path_.c_str(), O_RDONLY);
    if (fd_ < 0) {
        throw std::runtime_error("Failed to open file: " + log_path_ +
                                 " (error: " + std::string(strerror(errno)) + ")");
    }

    // Get file size
    struct stat st;
    if (fstat(fd_, &st) != 0) {
        close(fd_);
        throw std::runtime_error("Failed to stat file: " + log_path_);
    }
    file_size_ = st.st_size;

    if (file_size_ < FileHeader::SIZE) {
        close(fd_);
        throw std::runtime_error("File too small: " + log_path_);
    }

    // Memory-map the file
    mapped_data_ = static_cast<uint8_t*>(
        mmap(nullptr, file_size_, PROT_READ, MAP_SHARED, fd_, 0)
    );

    if (mapped_data_ == MAP_FAILED) {
        close(fd_);
        throw std::runtime_error("Failed to mmap file: " + log_path_);
    }

#ifdef __linux__
    // Linux-specific: hint kernel for sequential access
    // This doubles readahead window and improves cache efficiency
    madvise(mapped_data_, file_size_, MADV_SEQUENTIAL);
#endif

    // Parse and validate file header
    file_header_ = EventParser::parseFileHeader(mapped_data_, FileHeader::SIZE);
    offset_ = FileHeader::SIZE;
    is_open_ = true;
}

bool EventLogReader::readNext(Event& event) {
    if (!is_open_) {
        throw std::runtime_error("Reader not open");
    }

    // Check if at EOF
    if (offset_ >= file_size_) {
        return false;
    }

    // Need at least 24 bytes for header
    if (offset_ + 24 > file_size_) {
        return false;  // Incomplete header at EOF
    }

    // Read 24-byte fixed header
    const uint8_t* header_ptr = mapped_data_ + offset_;
    uint32_t payload_length = EventParser::readUint32LE(header_ptr + 20);

    // Calculate total event size
    size_t total_size = 28 + payload_length;

    // Ensure we have complete event
    if (offset_ + total_size > file_size_) {
        return false;  // Incomplete event at EOF
    }

    // Parse complete event
    event = EventParser::parse(mapped_data_ + offset_, total_size);
    offset_ += total_size;

    return true;
}

bool EventLogReader::remapIfGrown() {
    if (!is_open_) {
        return false;
    }

    // Get current file size
    struct stat st;
    if (fstat(fd_, &st) != 0) {
        return false;
    }

    size_t new_size = st.st_size;
    if (new_size <= file_size_) {
        return false;  // File hasn't grown
    }

    // Unmap old mapping
    if (munmap(mapped_data_, file_size_) != 0) {
        throw std::runtime_error("Failed to unmap file");
    }

    // Remap with new size
    file_size_ = new_size;
    mapped_data_ = static_cast<uint8_t*>(
        mmap(nullptr, file_size_, PROT_READ, MAP_SHARED, fd_, 0)
    );

    if (mapped_data_ == MAP_FAILED) {
        throw std::runtime_error("Failed to remap file");
    }

#ifdef __linux__
    madvise(mapped_data_, file_size_, MADV_SEQUENTIAL);
#endif

    return true;
}

}  // namespace trading_ledger
