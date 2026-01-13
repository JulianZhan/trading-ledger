#include "EventParser.h"
#include "FileReader.h"
#include <gtest/gtest.h>
#include <fstream>
#include <cstring>

using namespace trading_ledger;

// Helper to create test event bytes (standalone function for reuse)
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

class EventParserTest : public ::testing::Test {
};

TEST_F(EventParserTest, ParseValidEvent) {
    std::string payload = R"({"trade_id":"123","symbol":"AAPL"})";
    auto data = createTestEvent(1, 1234567890, EventType::TRADE_CREATED, payload);

    Event event = EventParser::parse(data.data(), data.size());

    EXPECT_EQ(event.sequence_num, 1);
    EXPECT_EQ(event.timestamp_ns, 1234567890);
    EXPECT_EQ(event.event_type, EventType::TRADE_CREATED);
    EXPECT_EQ(event.payload, payload);
}

TEST_F(EventParserTest, ParseEvent_EmptyPayload) {
    auto data = createTestEvent(42, 9999999999, EventType::TRADE_CREATED, "");

    Event event = EventParser::parse(data.data(), data.size());

    EXPECT_EQ(event.sequence_num, 42);
    EXPECT_EQ(event.timestamp_ns, 9999999999);
    EXPECT_EQ(event.payload, "");
}

TEST_F(EventParserTest, ParseEvent_LargePayload) {
    std::string large_payload(1000, 'X');
    auto data = createTestEvent(100, 555555, EventType::TRADE_CREATED, large_payload);

    Event event = EventParser::parse(data.data(), data.size());

    EXPECT_EQ(event.payload.size(), 1000);
    EXPECT_EQ(event.payload, large_payload);
}

TEST_F(EventParserTest, DetectCorruptedCRC) {
    std::string payload = R"({"trade_id":"456"})";
    auto data = createTestEvent(5, 777777, EventType::TRADE_CREATED, payload);

    // Corrupt the CRC (last 4 bytes)
    data[data.size() - 1] ^= 0xFF;

    EXPECT_THROW({
        EventParser::parse(data.data(), data.size());
    }, CorruptedEventException);
}

TEST_F(EventParserTest, InsufficientData) {
    std::vector<uint8_t> data(10);  // Too small

    EXPECT_THROW({
        EventParser::parse(data.data(), data.size());
    }, ParseException);
}

TEST_F(EventParserTest, ParseFileHeader_Valid) {
    std::vector<uint8_t> header(16);

    // Magic: 0x54524144 ("TRAD")
    header[0] = 0x44;
    header[1] = 0x41;
    header[2] = 0x52;
    header[3] = 0x54;

    // Version: 1
    header[4] = 0x01;
    header[5] = 0x00;
    header[6] = 0x00;
    header[7] = 0x00;

    // Reserved: 0
    for (int i = 8; i < 16; ++i) {
        header[i] = 0x00;
    }

    FileHeader fh = EventParser::parseFileHeader(header.data(), header.size());

    EXPECT_EQ(fh.magic, 0x54524144);
    EXPECT_EQ(fh.version, 1);
    EXPECT_TRUE(fh.isValid());
}

TEST_F(EventParserTest, ParseFileHeader_InvalidMagic) {
    std::vector<uint8_t> header(16, 0);
    header[0] = 0xFF;  // Wrong magic

    EXPECT_THROW({
        EventParser::parseFileHeader(header.data(), header.size());
    }, ParseException);
}

TEST_F(EventParserTest, CRC32_Consistency) {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05};

    uint32_t crc1 = EventParser::calculateCRC32(data.data(), data.size());
    uint32_t crc2 = EventParser::calculateCRC32(data.data(), data.size());

    EXPECT_EQ(crc1, crc2);
    EXPECT_NE(crc1, 0);  // Should not be zero for this data
}

class FileReaderTest : public ::testing::Test {
protected:
    std::string test_file_path = "/tmp/test_event_log.bin";

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

TEST_F(FileReaderTest, OpenAndValidateHeader) {
    createTestLogFile();

    FileReader reader(test_file_path);
    EXPECT_NO_THROW(reader.open());
}

TEST_F(FileReaderTest, ReadSequentialEvents) {
    createTestLogFile();

    FileReader reader(test_file_path);
    reader.open();

    // Read 3 events
    for (int i = 1; i <= 3; ++i) {
        Event event;
        ASSERT_TRUE(reader.readNext(event));
        EXPECT_EQ(event.sequence_num, i);
        EXPECT_EQ(event.timestamp_ns, i * 1000);
    }

    // Should reach EOF
    Event event;
    EXPECT_FALSE(reader.readNext(event));
}

TEST_F(FileReaderTest, OpenNonExistentFile) {
    FileReader reader("/nonexistent/path/file.bin");
    EXPECT_THROW(reader.open(), std::runtime_error);
}
