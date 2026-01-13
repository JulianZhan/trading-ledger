#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace trading_ledger {

// Event types matching Java Event.EventType
enum class EventType : uint8_t {
    TRADE_CREATED = 1,
    LEDGER_ENTRIES_GENERATED = 2,  // Future
    POSITION_UPDATED = 3            // Future
};

// Event record structure matching Java binary format
// Layout (little-endian):
//   Offset | Size | Field
//   -------|------|-------------
//   0      | 8    | sequence_num
//   8      | 8    | timestamp_ns
//   16     | 1    | event_type
//   17     | 3    | reserved (padding)
//   20     | 4    | payload_length
//   24     | N    | payload (JSON)
//   24+N   | 4    | crc32
struct Event {
    uint64_t sequence_num;
    uint64_t timestamp_ns;
    EventType event_type;
    std::string payload;  // JSON-encoded
    uint32_t crc32;

    // Calculate total record size in bytes (header + payload + CRC)
    size_t totalSize() const {
        return 28 + payload.size();  // 28-byte header + payload + 4-byte CRC (included in 28)
    }
};

// File header structure (16 bytes, written once at start of log)
struct FileHeader {
    uint32_t magic;      // 0x54524144 ("TRAD")
    uint32_t version;    // 1
    uint64_t reserved;   // 0x0000000000000000

    static constexpr uint32_t EXPECTED_MAGIC = 0x54524144;
    static constexpr uint32_t EXPECTED_VERSION = 1;
    static constexpr size_t SIZE = 16;

    bool isValid() const {
        return magic == EXPECTED_MAGIC && version == EXPECTED_VERSION;
    }
};

}  // namespace trading_ledger
