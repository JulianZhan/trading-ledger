#include "EventParser.h"
#include "FileReader.h"
#include <gtest/gtest.h>
#include <fstream>
#include <cstdlib>

using namespace trading_ledger;

class IntegrationTest : public ::testing::Test {
protected:
    // This test expects Java to have already written an event log
    // Path should match Java's EventLogConfig
    std::string event_log_path = "../data/event_log.bin";

    bool fileExists(const std::string& path) {
        std::ifstream f(path);
        return f.good();
    }
};

TEST_F(IntegrationTest, ReadJavaWrittenEventLog) {
    // Skip if event log doesn't exist (Java hasn't run yet)
    if (!fileExists(event_log_path)) {
        GTEST_SKIP() << "Event log not found. Run Java application first to generate: "
                     << event_log_path;
    }

    FileReader reader(event_log_path);
    ASSERT_NO_THROW(reader.open()) << "Failed to open Java-written event log";

    // Read at least one event
    Event event;
    ASSERT_TRUE(reader.readNext(event)) << "No events found in log";

    // Validate event structure
    EXPECT_GT(event.sequence_num, 0) << "Sequence number should be positive";
    EXPECT_GT(event.timestamp_ns, 0) << "Timestamp should be positive";
    EXPECT_EQ(event.event_type, EventType::TRADE_CREATED) << "Expected TRADE_CREATED event";
    EXPECT_FALSE(event.payload.empty()) << "Payload should not be empty";

    // Payload should be valid JSON (basic check: starts with '{' and ends with '}')
    EXPECT_EQ(event.payload.front(), '{') << "Payload should start with '{'";
    EXPECT_EQ(event.payload.back(), '}') << "Payload should end with '}'";

    // Count total events
    int count = 1;
    while (reader.readNext(event)) {
        EXPECT_EQ(event.event_type, EventType::TRADE_CREATED);
        count++;
    }

    std::cout << "Successfully read " << count << " events from Java-written log\n";
    EXPECT_GT(count, 0);
}

TEST_F(IntegrationTest, ValidateJavaEventPayload) {
    if (!fileExists(event_log_path)) {
        GTEST_SKIP() << "Event log not found. Run Java application first.";
    }

    FileReader reader(event_log_path);
    reader.open();

    Event event;
    if (reader.readNext(event)) {
        // Verify payload contains expected fields (basic string search)
        EXPECT_NE(event.payload.find("trade_id"), std::string::npos)
            << "Payload should contain 'trade_id' field";
        EXPECT_NE(event.payload.find("account_id"), std::string::npos)
            << "Payload should contain 'account_id' field";
        EXPECT_NE(event.payload.find("symbol"), std::string::npos)
            << "Payload should contain 'symbol' field";

        std::cout << "Sample event payload: " << event.payload.substr(0, 100) << "...\n";
    }
}
