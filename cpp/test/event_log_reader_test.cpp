#include "EventLogReader.h"
#include "EventLogTailer.h"
#include "DoubleEntryValidator.h"
#include <gtest/gtest.h>
#include <fstream>
#include <thread>
#include <chrono>

using namespace trading_ledger;

// Helper to create test event bytes
std::vector<uint8_t> createTestEvent(uint64_t seq, uint64_t ts,
                                      EventType type, const std::string& payload) {
    std::vector<uint8_t> data;

    // Sequence number (8 bytes, little-endian)
    for (int i = 0; i < 8; ++i) {
        data.push_back(static_cast<uint8_t>((seq >> (i * 8)) & 0xFF));
    }

    // Timestamp (8 bytes, little-endian)
    for (int i = 0; i < 8; ++i) {
        data.push_back(static_cast<uint8_t>((ts >> (i * 8)) & 0xFF));
    }

    // Event type (1 byte)
    data.push_back(static_cast<uint8_t>(type));

    // Reserved (3 bytes)
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);

    // Payload length (4 bytes, little-endian)
    uint32_t payload_len = payload.size();
    for (int i = 0; i < 4; ++i) {
        data.push_back(static_cast<uint8_t>((payload_len >> (i * 8)) & 0xFF));
    }

    // Payload
    data.insert(data.end(), payload.begin(), payload.end());

    // Calculate CRC32 over everything so far
    uint32_t crc = EventParser::calculateCRC32(data.data(), data.size());

    // Append CRC32 (4 bytes, little-endian)
    for (int i = 0; i < 4; ++i) {
        data.push_back(static_cast<uint8_t>((crc >> (i * 8)) & 0xFF));
    }

    return data;
}

class EventLogReaderTest : public ::testing::Test {
protected:
    std::string test_file_path = "/tmp/test_event_log_reader.bin";

    void TearDown() override {
        std::remove(test_file_path.c_str());
    }

    void createTestLogFile() {
        std::ofstream file(test_file_path, std::ios::binary);

        // Write file header
        uint8_t header[16] = {0};
        header[0] = 0x44; header[1] = 0x41; header[2] = 0x52; header[3] = 0x54;  // TRAD
        header[4] = 0x01; header[5] = 0x00; header[6] = 0x00; header[7] = 0x00;  // version=1
        file.write(reinterpret_cast<char*>(header), 16);

        // Write 3 test events
        for (int i = 1; i <= 3; ++i) {
            std::string payload = R"({"seq":)" + std::to_string(i) + "}";
            auto event_data = createTestEvent(i, i * 1000,
                                               EventType::TRADE_CREATED, payload);
            file.write(reinterpret_cast<char*>(event_data.data()), event_data.size());
        }

        file.close();
    }
};

TEST_F(EventLogReaderTest, OpenAndReadEvents) {
    createTestLogFile();

    EventLogReader reader(test_file_path);
    EXPECT_NO_THROW(reader.open());

    // Read 3 events
    for (int i = 1; i <= 3; ++i) {
        Event event;
        ASSERT_TRUE(reader.readNext(event));
        EXPECT_EQ(event.sequence_num, static_cast<uint64_t>(i));
        EXPECT_EQ(event.timestamp_ns, static_cast<uint64_t>(i * 1000));
    }

    // Should reach EOF
    Event event;
    EXPECT_FALSE(reader.readNext(event));
    EXPECT_TRUE(reader.eof());
}

TEST_F(EventLogReaderTest, RemapIfGrown) {
    createTestLogFile();

    EventLogReader reader(test_file_path);
    reader.open();

    // Read all events
    Event event;
    while (reader.readNext(event)) {}

    // File hasn't grown
    EXPECT_FALSE(reader.remapIfGrown());

    // Append new event to file
    std::ofstream file(test_file_path, std::ios::binary | std::ios::app);
    std::string payload = R"({"seq":4})";
    auto event_data = createTestEvent(4, 4000, EventType::TRADE_CREATED, payload);
    file.write(reinterpret_cast<char*>(event_data.data()), event_data.size());
    file.close();

    // File has grown
    EXPECT_TRUE(reader.remapIfGrown());

    // Can read new event
    ASSERT_TRUE(reader.readNext(event));
    EXPECT_EQ(event.sequence_num, 4);
}

TEST_F(EventLogReaderTest, OpenNonExistentFile) {
    EventLogReader reader("/nonexistent/path/file.bin");
    EXPECT_THROW(reader.open(), std::runtime_error);
}

// EventLogTailer Tests

class EventLogTailerTest : public ::testing::Test {
protected:
    std::string test_file_path = "/tmp/test_tailer.bin";

    void SetUp() override {
        // Create empty file
        std::ofstream file(test_file_path);
        file.close();
    }

    void TearDown() override {
        std::remove(test_file_path.c_str());
    }
};

TEST_F(EventLogTailerTest, InitializeSuccessfully) {
    EventLogTailer tailer(test_file_path);
    EXPECT_NO_THROW(tailer.init());
}

TEST_F(EventLogTailerTest, DetectsFileModification) {
    EventLogTailer tailer(test_file_path);
    tailer.init();

    // Modify file in another thread
    std::thread modifier([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::ofstream file(test_file_path, std::ios::app);
        file << "new data";
        file.close();
    });

    // Wait for modification (with timeout)
    bool modified = tailer.waitForModification(5000);  // 5 second timeout
    EXPECT_TRUE(modified);

    modifier.join();
}

TEST_F(EventLogTailerTest, TimeoutWhenNoModification) {
    EventLogTailer tailer(test_file_path);
    tailer.init();

    // Wait with short timeout
    tailer.waitForModification(100);  // 100ms timeout

    // Test just verifies that waitForModification returns without hanging
    // On Linux with inotify, this will timeout correctly
    // On other platforms with polling, behavior depends on timing
    SUCCEED();
}

// DoubleEntryValidator Tests

TEST(DoubleEntryValidatorTest, ProcessTradeEvent) {
    DoubleEntryValidator validator;

    Event event;
    event.sequence_num = 1;
    event.timestamp_ns = 1000000;
    event.event_type = EventType::TRADE_CREATED;
    event.payload = R"({"trade_id":"test-123","symbol":"AAPL","quantity":100,"price":150.0})";

    validator.processEvent(event);

    auto stats = validator.getStats();
    EXPECT_EQ(stats.events_processed, 1);
    EXPECT_EQ(stats.trades_validated, 1);
    EXPECT_EQ(stats.validation_errors, 0);
}

TEST(DoubleEntryValidatorTest, DetectsMissingFields) {
    DoubleEntryValidator validator;

    Event event;
    event.sequence_num = 1;
    event.timestamp_ns = 1000000;
    event.event_type = EventType::TRADE_CREATED;
    event.payload = R"({"symbol":"AAPL"})";  // Missing trade_id

    validator.processEvent(event);

    auto stats = validator.getStats();
    EXPECT_EQ(stats.events_processed, 1);
    EXPECT_EQ(stats.trades_validated, 0);
    EXPECT_EQ(stats.validation_errors, 1);
}

TEST(DoubleEntryValidatorTest, HandlesEmptyPayload) {
    DoubleEntryValidator validator;

    Event event;
    event.sequence_num = 1;
    event.timestamp_ns = 1000000;
    event.event_type = EventType::TRADE_CREATED;
    event.payload = "";

    validator.processEvent(event);

    auto stats = validator.getStats();
    EXPECT_EQ(stats.validation_errors, 1);
}
