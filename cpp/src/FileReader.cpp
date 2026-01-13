#include "FileReader.h"
#include "EventParser.h"
#include <stdexcept>
#include <vector>

namespace trading_ledger {

FileReader::FileReader(const std::string& file_path)
    : file_path_(file_path), offset_(0) {}

FileReader::~FileReader() {
    if (file_.is_open()) {
        file_.close();
    }
}

void FileReader::open() {
    file_.open(file_path_, std::ios::binary);
    if (!file_) {
        throw std::runtime_error("Failed to open file: " + file_path_);
    }

    // Read and validate file header
    uint8_t header_buf[FileHeader::SIZE];
    file_.read(reinterpret_cast<char*>(header_buf), FileHeader::SIZE);
    if (file_.gcount() != FileHeader::SIZE) {
        throw ParseException("Failed to read file header");
    }

    file_header_ = EventParser::parseFileHeader(header_buf, FileHeader::SIZE);
    offset_ = FileHeader::SIZE;
}

bool FileReader::readNext(Event& event) {
    if (!file_.is_open()) {
        throw std::runtime_error("File not opened");
    }

    if (file_.eof()) {
        return false;
    }

    // Read header first to determine payload size
    uint8_t header_buf[24];
    file_.read(reinterpret_cast<char*>(header_buf), 24);
    if (file_.gcount() == 0) {
        return false;  // EOF
    }
    if (file_.gcount() != 24) {
        throw ParseException("Incomplete event header");
    }

    // Parse payload length
    uint32_t payload_length = EventParser::readUint32LE(header_buf + 20);

    // Read payload + CRC
    size_t remaining = payload_length + 4;  // payload + CRC32
    std::vector<uint8_t> full_record(28 + payload_length);
    std::memcpy(full_record.data(), header_buf, 24);

    file_.read(reinterpret_cast<char*>(full_record.data() + 24), remaining);
    if (static_cast<size_t>(file_.gcount()) != remaining) {
        throw ParseException("Incomplete event payload");
    }

    // Parse full event
    event = EventParser::parse(full_record.data(), full_record.size());
    offset_ += full_record.size();

    return true;
}

bool FileReader::eof() const {
    return file_.eof();
}

}  // namespace trading_ledger
