#pragma once

#include "Event.h"
#include <stdexcept>

namespace trading_ledger {

class ParseException : public std::runtime_error {
public:
    explicit ParseException(const std::string& msg) : std::runtime_error(msg) {}
};

class CorruptedEventException : public ParseException {
public:
    explicit CorruptedEventException(const std::string& msg)
        : ParseException("CRC32 mismatch: " + msg) {}
};

class EventParser {
public:
    // Parse event from byte buffer (little-endian format)
    // Throws ParseException if data is insufficient
    // Throws CorruptedEventException if CRC32 mismatch
    static Event parse(const uint8_t* data, size_t length);

    // Parse file header
    static FileHeader parseFileHeader(const uint8_t* data, size_t length);

    // Calculate CRC32 checksum (compatible with Java's CRC32)
    static uint32_t calculateCRC32(const uint8_t* data, size_t length);

    // Read little-endian integers (public for use by FileReader)
    static uint16_t readUint16LE(const uint8_t* data);
    static uint32_t readUint32LE(const uint8_t* data);
    static uint64_t readUint64LE(const uint8_t* data);
};

}  // namespace trading_ledger
