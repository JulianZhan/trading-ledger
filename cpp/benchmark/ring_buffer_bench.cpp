#include "RingBuffer.h"
#include <benchmark/benchmark.h>
#include <thread>
#include <atomic>
#include <chrono>

using namespace trading_ledger;

// Benchmark: Single-threaded push/pop throughput
static void BM_RingBuffer_SingleThreaded(benchmark::State& state) {
    RingBuffer<int, 1024> buffer;
    int value = 42;

    for (auto _ : state) {
        buffer.try_push(value);
        int result;
        buffer.try_pop(result);
        benchmark::DoNotOptimize(result);
    }

    state.SetItemsProcessed(state.iterations() * 2);  // push + pop
}
BENCHMARK(BM_RingBuffer_SingleThreaded);

// Benchmark: Producer throughput (with slow consumer)
static void BM_RingBuffer_ProducerThroughput(benchmark::State& state) {
    RingBuffer<int, 1024> buffer;

    std::atomic<bool> stop{false};
    std::atomic<size_t> consumed{0};

    // Slow consumer thread
    std::thread consumer([&]() {
        int item;
        while (!stop.load(std::memory_order_acquire)) {
            if (buffer.try_pop(item)) {
                consumed.fetch_add(1, std::memory_order_relaxed);
                benchmark::DoNotOptimize(item);
            }
        }
        // Drain remaining
        while (buffer.try_pop(item)) {
            consumed.fetch_add(1, std::memory_order_relaxed);
        }
    });

    // Producer (benchmark)
    size_t produced = 0;
    for (auto _ : state) {
        while (!buffer.try_push(42)) {
            // Spin on full buffer
        }
        ++produced;
    }

    stop.store(true, std::memory_order_release);
    consumer.join();

    state.SetItemsProcessed(produced);
    state.counters["consumed"] = consumed.load();
}
BENCHMARK(BM_RingBuffer_ProducerThroughput)->Threads(1);

// Benchmark: Consumer throughput (with fast producer)
static void BM_RingBuffer_ConsumerThroughput(benchmark::State& state) {
    RingBuffer<int, 1024> buffer;

    std::atomic<bool> stop{false};

    // Fast producer thread (pre-fill and keep filling)
    std::thread producer([&]() {
        int value = 0;
        while (!stop.load(std::memory_order_acquire)) {
            if (buffer.try_push(value++)) {
                // Successfully pushed
            }
        }
    });

    // Wait for buffer to fill
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Consumer (benchmark)
    size_t consumed = 0;
    for (auto _ : state) {
        int item;
        while (!buffer.try_pop(item)) {
            // Spin on empty buffer
        }
        benchmark::DoNotOptimize(item);
        ++consumed;
    }

    stop.store(true, std::memory_order_release);
    producer.join();

    state.SetItemsProcessed(consumed);
}
BENCHMARK(BM_RingBuffer_ConsumerThroughput)->Threads(1);

// Benchmark: SPSC throughput (balanced producer/consumer)
static void BM_RingBuffer_SPSCThroughput(benchmark::State& state) {
    constexpr size_t NUM_ITEMS = 1000000;
    RingBuffer<int, 1024> buffer;

    for (auto _ : state) {
        state.PauseTiming();

        std::atomic<bool> producer_done{false};
        std::atomic<size_t> items_consumed{0};

        state.ResumeTiming();

        // Producer thread
        std::thread producer([&]() {
            for (size_t i = 0; i < NUM_ITEMS; ++i) {
                while (!buffer.try_push(static_cast<int>(i))) {
                    // Spin
                }
            }
            producer_done.store(true, std::memory_order_release);
        });

        // Consumer thread
        std::thread consumer([&]() {
            size_t count = 0;
            while (count < NUM_ITEMS) {
                int item;
                if (buffer.try_pop(item)) {
                    benchmark::DoNotOptimize(item);
                    ++count;
                }
            }
            items_consumed.store(count, std::memory_order_release);
        });

        producer.join();
        consumer.join();

        state.PauseTiming();
        benchmark::DoNotOptimize(items_consumed.load());
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * NUM_ITEMS);
}
BENCHMARK(BM_RingBuffer_SPSCThroughput);

// Benchmark: Latency (measure time from push to pop)
static void BM_RingBuffer_PushPopLatency(benchmark::State& state) {
    RingBuffer<std::chrono::nanoseconds, 1024> buffer;

    std::atomic<bool> stop{false};
    std::atomic<int64_t> total_latency_ns{0};
    std::atomic<size_t> samples{0};

    // Consumer measures latency
    std::thread consumer([&]() {
        std::chrono::nanoseconds timestamp;
        while (!stop.load(std::memory_order_acquire) || !buffer.empty()) {
            if (buffer.try_pop(timestamp)) {
                auto now = std::chrono::steady_clock::now();
                auto latency = now - std::chrono::time_point<std::chrono::steady_clock>(timestamp);
                total_latency_ns.fetch_add(latency.count(), std::memory_order_relaxed);
                samples.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });

    // Producer (benchmark)
    for (auto _ : state) {
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        while (!buffer.try_push(now)) {
            // Spin
        }
    }

    stop.store(true, std::memory_order_release);
    consumer.join();

    if (samples.load() > 0) {
        double avg_latency_ns = static_cast<double>(total_latency_ns.load()) / samples.load();
        state.counters["avg_latency_ns"] = avg_latency_ns;
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_RingBuffer_PushPopLatency);

// Benchmark: Impact of buffer size
static void BM_RingBuffer_BufferSizeImpact(benchmark::State& state) {
    // Note: buffer_size parameter for labeling only
    // Template requires compile-time constant, so we use fixed size
    RingBuffer<int, 1024> buffer;

    std::atomic<bool> stop{false};

    std::thread consumer([&]() {
        int item;
        while (!stop.load(std::memory_order_acquire)) {
            buffer.try_pop(item);
            benchmark::DoNotOptimize(item);
        }
    });

    for (auto _ : state) {
        while (!buffer.try_push(42)) {
            // Spin
        }
    }

    stop.store(true, std::memory_order_release);
    consumer.join();

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_RingBuffer_BufferSizeImpact)->Arg(64)->Arg(256)->Arg(1024)->Arg(4096);

// Benchmark: Empty/full detection overhead
static void BM_RingBuffer_EmptyCheck(benchmark::State& state) {
    RingBuffer<int, 1024> buffer;

    for (auto _ : state) {
        bool empty = buffer.empty();
        benchmark::DoNotOptimize(empty);
    }
}
BENCHMARK(BM_RingBuffer_EmptyCheck);

// Benchmark: Size query overhead
static void BM_RingBuffer_SizeQuery(benchmark::State& state) {
    RingBuffer<int, 1024> buffer;
    buffer.try_push(42);  // Make non-empty

    for (auto _ : state) {
        size_t size = buffer.size();
        benchmark::DoNotOptimize(size);
    }
}
BENCHMARK(BM_RingBuffer_SizeQuery);

// Benchmark: Move semantics overhead
struct LargeObject {
    std::array<int, 64> data;
    LargeObject() { data.fill(42); }
};

static void BM_RingBuffer_MoveSemantics(benchmark::State& state) {
    RingBuffer<LargeObject, 256> buffer;

    for (auto _ : state) {
        LargeObject obj;
        buffer.try_push(std::move(obj));
        LargeObject result;
        buffer.try_pop(result);
        benchmark::DoNotOptimize(result);
    }

    state.SetItemsProcessed(state.iterations() * 2);
}
BENCHMARK(BM_RingBuffer_MoveSemantics);

BENCHMARK_MAIN();
