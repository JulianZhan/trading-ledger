#include "EventParser.h"
#include <benchmark/benchmark.h>
#include <vector>
#include <cstring>

using namespace trading_ledger;

// Helper to create test event
std::vector<uint8_t> createBenchEvent(const std::string& payload) {
    std::vector<uint8_t> data;

    uint64_t seq = 12345;
    uint64_t ts = 9876543210;

    // Sequence number (8 bytes, little-endian)
    for (int i = 0; i < 8; ++i) {
        data.push_back(static_cast<uint8_t>((seq >> (i * 8)) & 0xFF));
    }

    // Timestamp (8 bytes, little-endian)
    for (int i = 0; i < 8; ++i) {
        data.push_back(static_cast<uint8_t>((ts >> (i * 8)) & 0xFF));
    }

    // Event type (1 byte)
    data.push_back(static_cast<uint8_t>(EventType::TRADE_CREATED));

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

    // Calculate CRC32
    uint32_t crc = EventParser::calculateCRC32(data.data(), data.size());

    // Append CRC32 (4 bytes, little-endian)
    for (int i = 0; i < 4; ++i) {
        data.push_back(static_cast<uint8_t>((crc >> (i * 8)) & 0xFF));
    }

    return data;
}

static void BM_ParseEvent_SmallPayload(benchmark::State& state) {
    std::string payload = R"({"trade_id":"abc123","symbol":"AAPL","qty":100})";
    auto data = createBenchEvent(payload);

    for (auto _ : state) {
        Event event = EventParser::parse(data.data(), data.size());
        benchmark::DoNotOptimize(event);
    }

    state.SetBytesProcessed(state.iterations() * data.size());
}
BENCHMARK(BM_ParseEvent_SmallPayload);

static void BM_ParseEvent_LargePayload(benchmark::State& state) {
    std::string payload(1024, 'X');  // 1KB payload
    auto data = createBenchEvent(payload);

    for (auto _ : state) {
        Event event = EventParser::parse(data.data(), data.size());
        benchmark::DoNotOptimize(event);
    }

    state.SetBytesProcessed(state.iterations() * data.size());
}
BENCHMARK(BM_ParseEvent_LargePayload);

static void BM_CRC32_Calculation(benchmark::State& state) {
    size_t size = state.range(0);
    std::vector<uint8_t> data(size, 0xAB);

    for (auto _ : state) {
        uint32_t crc = EventParser::calculateCRC32(data.data(), data.size());
        benchmark::DoNotOptimize(crc);
    }

    state.SetBytesProcessed(state.iterations() * size);
}
BENCHMARK(BM_CRC32_Calculation)->Range(64, 8192);
