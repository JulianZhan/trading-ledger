#include "EventLogReader.h"
#include "EventLogTailer.h"
#include "RingBuffer.h"
#include "DoubleEntryValidator.h"
#include "LatencyHistogram.h"
#include <thread>
#include <atomic>
#include <iostream>
#include <csignal>
#include <cstring>
#include <chrono>

using namespace trading_ledger;

// Global flag for graceful shutdown
static std::atomic<bool> g_running{true};

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived shutdown signal, stopping gracefully..." << std::endl;
        g_running.store(false, std::memory_order_release);
    }
}

/**
 * Producer thread: reads events from log and pushes to ring buffer
 */
void producerThread(const std::string& log_path,
                    RingBuffer<Event, 4096>& buffer,
                    std::atomic<size_t>& events_read) {
    try {
        EventLogReader reader(log_path);
        reader.open();

        EventLogTailer tailer(log_path);
        tailer.init();

        std::cout << "Producer: Using "
                  << (tailer.isUsingInotify() ? "inotify (Linux)" : "polling (fallback)")
                  << " for tail-following" << std::endl;

        while (g_running.load(std::memory_order_acquire)) {
            Event event;

            // Try to read next event
            if (reader.readNext(event)) {
                // Push to ring buffer (spin if full)
                while (!buffer.try_push(std::move(event))) {
                    if (!g_running.load(std::memory_order_acquire)) {
                        return;
                    }
                    std::this_thread::yield();
                }

                events_read.fetch_add(1, std::memory_order_relaxed);
            } else {
                // EOF reached, wait for file to grow
                if (!reader.remapIfGrown()) {
                    // File hasn't grown, wait for modification
                    tailer.waitForModification(100);  // 100ms timeout
                    reader.remapIfGrown();  // Try again
                }
            }
        }

        std::cout << "Producer: Shutting down" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Producer error: " << e.what() << std::endl;
        g_running.store(false, std::memory_order_release);
    }
}

/**
 * Consumer thread: pops events from buffer and validates
 */
void consumerThread(RingBuffer<Event, 4096>& buffer,
                    std::atomic<size_t>& events_processed,
                    LatencyHistogram& latency_histogram) {
    try {
        DoubleEntryValidator validator;

        while (g_running.load(std::memory_order_acquire) || !buffer.empty()) {
            Event event;

            if (buffer.try_pop(event)) {
                // Measure processing latency
                auto start = std::chrono::steady_clock::now();

                // Process event
                validator.processEvent(event);
                events_processed.fetch_add(1, std::memory_order_relaxed);

                // Record latency
                auto end = std::chrono::steady_clock::now();
                auto latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
                latency_histogram.record(latency_ns);

                // Print periodic latency summary every 10,000 events
                if (events_processed.load(std::memory_order_relaxed) % 10000 == 0) {
                    latency_histogram.printSummary();
                    latency_histogram.clear();  // Reset for next window
                }
            } else {
                // Buffer empty, yield
                std::this_thread::yield();
            }
        }

        // Print final summaries
        std::cout << "\n=== Final Statistics ===" << std::endl;
        validator.printSummary();
        if (latency_histogram.count() > 0) {
            latency_histogram.printSummary();
        }

        std::cout << "\nConsumer: Shutting down" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Consumer error: " << e.what() << std::endl;
        g_running.store(false, std::memory_order_release);
    }
}

/**
 * Monitor thread: prints progress
 */
void monitorThread(std::atomic<size_t>& events_read,
                   std::atomic<size_t>& events_processed) {
    size_t last_read = 0;
    size_t last_processed = 0;

    while (g_running.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        size_t current_read = events_read.load(std::memory_order_relaxed);
        size_t current_processed = events_processed.load(std::memory_order_relaxed);

        size_t read_rate = (current_read - last_read) / 5;
        size_t process_rate = (current_processed - last_processed) / 5;

        std::cout << "Monitor: Read " << current_read << " events ("
                  << read_rate << " events/sec), Processed " << current_processed
                  << " (" << process_rate << " events/sec)" << std::endl;

        last_read = current_read;
        last_processed = current_processed;
    }
}

int main(int argc, char** argv) {
    // Parse command line arguments
    std::string log_path = "../data/event_log.bin";  // Default path

    if (argc > 1) {
        log_path = argv[1];
    }

    std::cout << "Event Processor Starting..." << std::endl;
    std::cout << "Log path: " << log_path << std::endl;

    // Set up signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Create ring buffer and latency histogram
    RingBuffer<Event, 4096> buffer;
    LatencyHistogram latency_histogram;

    // Atomic counters
    std::atomic<size_t> events_read{0};
    std::atomic<size_t> events_processed{0};

    // Start threads
    std::thread producer(producerThread, log_path, std::ref(buffer), std::ref(events_read));
    std::thread consumer(consumerThread, std::ref(buffer), std::ref(events_processed), std::ref(latency_histogram));
    std::thread monitor(monitorThread, std::ref(events_read), std::ref(events_processed));

    // Wait for threads to complete
    producer.join();
    consumer.join();

    g_running.store(false, std::memory_order_release);
    monitor.join();

    std::cout << "\nEvent Processor Shutdown Complete" << std::endl;
    std::cout << "Total events read: " << events_read.load() << std::endl;
    std::cout << "Total events processed: " << events_processed.load() << std::endl;

    return 0;
}
