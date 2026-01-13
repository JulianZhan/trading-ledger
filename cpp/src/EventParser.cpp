#include "EventParser.h"
#include <zlib.h>
#include <cstring>
#include <sstream>

namespace trading_ledger {

uint16_t EventParser::readUint16LE(const uint8_t* data) {
    return static_cast<uint16_t>(data[0]) |
           (static_cast<uint16_t>(data[1]) << 8);
}

uint32_t EventParser::readUint32LE(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

uint64_t EventParser::readUint64LE(const uint8_t* data) {
    return static_cast<uint64_t>(data[0]) |
           (static_cast<uint64_t>(data[1]) << 8) |
           (static_cast<uint64_t>(data[2]) << 16) |
           (static_cast<uint64_t>(data[3]) << 24) |
           (static_cast<uint64_t>(data[4]) << 32) |
           (static_cast<uint64_t>(data[5]) << 40) |
           (static_cast<uint64_t>(data[6]) << 48) |
           (static_cast<uint64_t>(data[7]) << 56);
}

uint32_t EventParser::calculateCRC32(const uint8_t* data, size_t length) {
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, data, length);
    return static_cast<uint32_t>(crc);
}

FileHeader EventParser::parseFileHeader(const uint8_t* data, size_t length) {
    if (length < FileHeader::SIZE) {
        throw ParseException("Insufficient data for file header");
    }

    FileHeader header;
    header.magic = readUint32LE(data);
    header.version = readUint32LE(data + 4);
    header.reserved = readUint64LE(data + 8);

    if (!header.isValid()) {
        std::ostringstream oss;
        oss << "Invalid file header: magic=0x" << std::hex << header.magic
            << ", version=" << std::dec << header.version;
        throw ParseException(oss.str());
    }

    return header;
}

Event EventParser::parse(const uint8_t* data, size_t length) {
    // Minimum size: 28 bytes (header) + 4 bytes (CRC)
    if (length < 28) {
        throw ParseException("Insufficient data for event record");
    }

    // Read header fields (24 bytes)
    Event event;
    event.sequence_num = readUint64LE(data);
    event.timestamp_ns = readUint64LE(data + 8);
    event.event_type = static_cast<EventType>(data[16]);
    // Skip reserved bytes [17-19]
    uint32_t payload_length = readUint32LE(data + 20);

    // Validate total length
    size_t expected_total = 28 + payload_length;
    if (length < expected_total) {
        std::ostringstream oss;
        oss << "Insufficient data: expected " << expected_total
            << " bytes, got " << length;
        throw ParseException(oss.str());
    }

    // Read payload
    event.payload.assign(reinterpret_cast<const char*>(data + 24), payload_length);

    // Read CRC32 (last 4 bytes)
    uint32_t stored_crc = readUint32LE(data + 24 + payload_length);
    event.crc32 = stored_crc;

    // Verify CRC32 (calculate over everything except CRC itself)
    uint32_t calculated_crc = calculateCRC32(data, 24 + payload_length);
    if (calculated_crc != stored_crc) {
        std::ostringstream oss;
        oss << "calculated=0x" << std::hex << calculated_crc
            << ", stored=0x" << stored_crc;
        throw CorruptedEventException(oss.str());
    }

    return event;
}

}  // namespace trading_ledger
