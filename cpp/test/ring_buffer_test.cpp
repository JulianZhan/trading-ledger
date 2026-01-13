#include "RingBuffer.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <numeric>
#include <chrono>

using namespace trading_ledger;

// Test fixture
class RingBufferTest : public ::testing::Test {
protected:
    // Use small buffer for testing edge cases
    static constexpr size_t BUFFER_SIZE = 8;
    RingBuffer<int, BUFFER_SIZE> buffer;
};

// Basic functionality tests

TEST_F(RingBufferTest, InitiallyEmpty) {
    EXPECT_TRUE(buffer.empty());
    EXPECT_EQ(buffer.size(), 0);
    EXPECT_EQ(buffer.capacity(), BUFFER_SIZE - 1);  // One slot reserved
}

TEST_F(RingBufferTest, PushSingleItem) {
    EXPECT_TRUE(buffer.try_push(42));
    EXPECT_FALSE(buffer.empty());
    EXPECT_EQ(buffer.size(), 1);
}

TEST_F(RingBufferTest, PopSingleItem) {
    buffer.try_push(42);

    int item;
    EXPECT_TRUE(buffer.try_pop(item));
    EXPECT_EQ(item, 42);
    EXPECT_TRUE(buffer.empty());
}

TEST_F(RingBufferTest, PopFromEmptyReturnsFalse) {
    int item;
    EXPECT_FALSE(buffer.try_pop(item));
}

TEST_F(RingBufferTest, PushToFullReturnsFalse) {
    // Fill buffer (capacity = SIZE - 1)
    for (size_t i = 0; i < buffer.capacity(); ++i) {
        EXPECT_TRUE(buffer.try_push(static_cast<int>(i)));
    }

    // Next push should fail
    EXPECT_FALSE(buffer.try_push(999));
}

TEST_F(RingBufferTest, PushPopSequence) {
    std::vector<int> input = {1, 2, 3, 4, 5};

    // Push all items
    for (int val : input) {
        EXPECT_TRUE(buffer.try_push(val));
    }

    // Pop and verify
    for (int expected : input) {
        int actual;
        EXPECT_TRUE(buffer.try_pop(actual));
        EXPECT_EQ(actual, expected);
    }

    EXPECT_TRUE(buffer.empty());
}

TEST_F(RingBufferTest, WrapAroundIndexing) {
    // Fill buffer
    for (size_t i = 0; i < buffer.capacity(); ++i) {
        EXPECT_TRUE(buffer.try_push(static_cast<int>(i)));
    }

    // Pop half
    for (size_t i = 0; i < buffer.capacity() / 2; ++i) {
        int item;
        EXPECT_TRUE(buffer.try_pop(item));
        EXPECT_EQ(item, static_cast<int>(i));
    }

    // Push new items (will wrap around)
    for (size_t i = 0; i < buffer.capacity() / 2; ++i) {
        EXPECT_TRUE(buffer.try_push(static_cast<int>(i + 1000)));
    }

    // Verify remaining old items
    for (size_t i = buffer.capacity() / 2; i < buffer.capacity(); ++i) {
        int item;
        EXPECT_TRUE(buffer.try_pop(item));
        EXPECT_EQ(item, static_cast<int>(i));
    }

    // Verify new items
    for (size_t i = 0; i < buffer.capacity() / 2; ++i) {
        int item;
        EXPECT_TRUE(buffer.try_pop(item));
        EXPECT_EQ(item, static_cast<int>(i + 1000));
    }

    EXPECT_TRUE(buffer.empty());
}

TEST_F(RingBufferTest, MoveSemantics) {
    struct MoveOnlyType {
        int value;
        std::unique_ptr<int> ptr;

        // Default constructor (needed for array initialization)
        MoveOnlyType() : value(0), ptr(nullptr) {}

        MoveOnlyType(int v) : value(v), ptr(std::make_unique<int>(v * 10)) {}

        // Move constructor
        MoveOnlyType(MoveOnlyType&& other) noexcept
            : value(other.value), ptr(std::move(other.ptr)) {}

        // Move assignment
        MoveOnlyType& operator=(MoveOnlyType&& other) noexcept {
            value = other.value;
            ptr = std::move(other.ptr);
            return *this;
        }

        // Non-copyable
        MoveOnlyType(const MoveOnlyType&) = delete;
        MoveOnlyType& operator=(const MoveOnlyType&) = delete;
    };

    RingBuffer<MoveOnlyType, 8> move_buffer;

    // Push with move
    MoveOnlyType item(42);
    EXPECT_TRUE(move_buffer.try_push(std::move(item)));

    // Pop
    MoveOnlyType result;
    EXPECT_TRUE(move_buffer.try_pop(result));
    EXPECT_EQ(result.value, 42);
    EXPECT_NE(result.ptr, nullptr);
    EXPECT_EQ(*result.ptr, 420);
}

TEST_F(RingBufferTest, SizeAccuracyAfterOperations) {
    EXPECT_EQ(buffer.size(), 0);

    buffer.try_push(1);
    EXPECT_EQ(buffer.size(), 1);

    buffer.try_push(2);
    EXPECT_EQ(buffer.size(), 2);

    int item;
    buffer.try_pop(item);
    EXPECT_EQ(buffer.size(), 1);

    buffer.try_pop(item);
    EXPECT_EQ(buffer.size(), 0);
}

// Multi-threaded SPSC tests

TEST_F(RingBufferTest, SingleProducerSingleConsumer) {
    constexpr size_t NUM_ITEMS = 100000;
    RingBuffer<int, 1024> large_buffer;

    std::atomic<bool> producer_done{false};
    std::vector<int> consumed;
    consumed.reserve(NUM_ITEMS);

    // Producer thread
    std::thread producer([&]() {
        for (size_t i = 0; i < NUM_ITEMS; ++i) {
            while (!large_buffer.try_push(static_cast<int>(i))) {
                // Spin until space available
                std::this_thread::yield();
            }
        }
        producer_done.store(true, std::memory_order_release);
    });

    // Consumer thread
    std::thread consumer([&]() {
        size_t count = 0;
        while (count < NUM_ITEMS) {
            int item;
            if (large_buffer.try_pop(item)) {
                consumed.push_back(item);
                ++count;
            } else {
                // Buffer empty, yield
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    // Verify all items received in order
    EXPECT_EQ(consumed.size(), NUM_ITEMS);
    for (size_t i = 0; i < NUM_ITEMS; ++i) {
        EXPECT_EQ(consumed[i], static_cast<int>(i));
    }
}

TEST_F(RingBufferTest, SPSCWithBackpressure) {
    constexpr size_t NUM_ITEMS = 10000;
    RingBuffer<int, 16> small_buffer;  // Small buffer = more contention

    std::atomic<size_t> push_failures{0};
    std::atomic<size_t> pop_failures{0};

    // Producer thread
    std::thread producer([&]() {
        for (size_t i = 0; i < NUM_ITEMS; ++i) {
            while (!small_buffer.try_push(static_cast<int>(i))) {
                ++push_failures;
                std::this_thread::yield();
            }
        }
    });

    // Consumer thread
    std::thread consumer([&]() {
        size_t count = 0;
        while (count < NUM_ITEMS) {
            int item;
            if (small_buffer.try_pop(item)) {
                EXPECT_EQ(item, static_cast<int>(count));
                ++count;
            } else {
                ++pop_failures;
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    // With small buffer, expect many push failures (backpressure)
    EXPECT_GT(push_failures.load(), 0);

    // Expect some pop failures (buffer empty)
    EXPECT_GT(pop_failures.load(), 0);

    // Buffer should be empty at end
    EXPECT_TRUE(small_buffer.empty());
}

TEST_F(RingBufferTest, SPSCHighContentionStressTest) {
    constexpr size_t NUM_ITEMS = 1000000;
    RingBuffer<uint64_t, 512> stress_buffer;

    std::atomic<bool> producer_done{false};
    std::atomic<uint64_t> checksum_produced{0};
    std::atomic<uint64_t> checksum_consumed{0};

    // Producer: write 0, 1, 2, ..., NUM_ITEMS-1
    std::thread producer([&]() {
        for (uint64_t i = 0; i < NUM_ITEMS; ++i) {
            while (!stress_buffer.try_push(i)) {
                std::this_thread::yield();
            }
            checksum_produced.fetch_add(i, std::memory_order_relaxed);
        }
        producer_done.store(true, std::memory_order_release);
    });

    // Consumer: read and verify
    std::thread consumer([&]() {
        uint64_t expected = 0;
        while (expected < NUM_ITEMS) {
            uint64_t item;
            if (stress_buffer.try_pop(item)) {
                EXPECT_EQ(item, expected);
                checksum_consumed.fetch_add(item, std::memory_order_relaxed);
                ++expected;
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    // Verify checksums match
    EXPECT_EQ(checksum_produced.load(), checksum_consumed.load());

    // Expected checksum: sum of 0..NUM_ITEMS-1 = NUM_ITEMS*(NUM_ITEMS-1)/2
    uint64_t expected_checksum = NUM_ITEMS * (NUM_ITEMS - 1) / 2;
    EXPECT_EQ(checksum_consumed.load(), expected_checksum);
}

// Power-of-2 size enforcement

TEST(RingBufferStaticTest, PowerOfTwoEnforcement) {
    // These should compile
    RingBuffer<int, 2> buf2;
    RingBuffer<int, 4> buf4;
    RingBuffer<int, 8> buf8;
    RingBuffer<int, 1024> buf1024;

    // These should NOT compile (uncomment to verify):
    // RingBuffer<int, 3> buf3;    // static_assert failure
    // RingBuffer<int, 5> buf5;    // static_assert failure
    // RingBuffer<int, 0> buf0;    // static_assert failure

    SUCCEED();
}

// Cache-line alignment verification

TEST(RingBufferStaticTest, CacheLineAlignment) {
    RingBuffer<int, 64> buf;

    // Check alignment (implementation detail, but useful to verify)
    // alignas(64) should align head_, tail_, buffer_ to 64-byte boundaries

    // This test just ensures buffer compiles and runs
    buf.try_push(42);
    int item;
    buf.try_pop(item);

    SUCCEED();
}

// Edge case: Exactly capacity items

TEST_F(RingBufferTest, ExactlyCapacityItems) {
    // Fill to exact capacity
    for (size_t i = 0; i < buffer.capacity(); ++i) {
        EXPECT_TRUE(buffer.try_push(static_cast<int>(i)));
    }

    // Buffer should be full now
    EXPECT_FALSE(buffer.try_push(999));
    EXPECT_EQ(buffer.size(), buffer.capacity());

    // Pop all
    for (size_t i = 0; i < buffer.capacity(); ++i) {
        int item;
        EXPECT_TRUE(buffer.try_pop(item));
        EXPECT_EQ(item, static_cast<int>(i));
    }

    // Buffer should be empty
    EXPECT_TRUE(buffer.empty());
    int item;
    EXPECT_FALSE(buffer.try_pop(item));
}
