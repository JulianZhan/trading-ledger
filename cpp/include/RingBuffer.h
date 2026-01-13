#pragma once

#include <atomic>
#include <array>
#include <type_traits>
#include <cstddef>

namespace trading_ledger {

/**
 * Lock-free Single-Producer Single-Consumer (SPSC) ring buffer
 *
 * Thread-safety:
 * - One thread calls try_push() (producer)
 * - One thread calls try_pop() (consumer)
 * - No synchronization needed beyond atomic operations
 *
 * Memory ordering:
 * - acquire on loads: synchronizes-with prior release stores
 * - release on stores: publishes all prior writes
 * - relaxed for local thread reads: no synchronization needed
 *
 * Cache-line alignment:
 * - alignas(64): prevents false sharing on x86-64 (typical 64-byte cache line)
 * - head, tail, and buffer each own their cache lines
 *
 * Power-of-2 sizing:
 * - Enables branchless modulo using bit-mask: (index & (SIZE-1))
 * - Faster than division/modulo instruction
 */
template<typename T, size_t SIZE>
class RingBuffer {
    static_assert((SIZE & (SIZE - 1)) == 0, "SIZE must be power of 2");
    static_assert(SIZE > 0, "SIZE must be greater than 0");

public:
    RingBuffer() : head_(0), tail_(0), buffer_{} {}

    // Non-copyable, non-movable (contains atomics)
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;
    RingBuffer(RingBuffer&&) = delete;
    RingBuffer& operator=(RingBuffer&&) = delete;

    /**
     * Producer: Push item to buffer (non-blocking)
     *
     * @param item Item to push
     * @return true if pushed successfully, false if buffer full
     *
     * Memory ordering:
     * - Load head with acquire: ensure we see consumer's latest progress
     * - Store tail with release: publish new item to consumer
     */
    bool try_push(const T& item) {
        // Load current tail (producer's write position)
        // Relaxed: only producer writes tail, no synchronization needed
        size_t current_tail = tail_.load(std::memory_order_relaxed);

        // Calculate next tail position (wrap around using bit mask)
        size_t next_tail = (current_tail + 1) & (SIZE - 1);

        // Check if buffer is full
        // Acquire: synchronize-with consumer's release store to head
        // Ensures we see all items consumer has popped
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;  // Full
        }

        // Write item to buffer
        buffer_[current_tail] = item;

        // Publish new tail position
        // Release: ensure item write happens-before this store
        // Consumer will see item when it reads this tail value
        tail_.store(next_tail, std::memory_order_release);

        return true;
    }

    /**
     * Producer: Push item to buffer (move semantics)
     */
    bool try_push(T&& item) {
        size_t current_tail = tail_.load(std::memory_order_relaxed);
        size_t next_tail = (current_tail + 1) & (SIZE - 1);

        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;  // Full
        }

        buffer_[current_tail] = std::move(item);
        tail_.store(next_tail, std::memory_order_release);

        return true;
    }

    /**
     * Consumer: Pop item from buffer (non-blocking)
     *
     * @param item Output parameter to store popped item
     * @return true if popped successfully, false if buffer empty
     *
     * Memory ordering:
     * - Load tail with acquire: synchronize with producer's release store
     * - Store head with release: publish progress to producer
     */
    bool try_pop(T& item) {
        // Load current head (consumer's read position)
        // Relaxed: only consumer writes head, no synchronization needed
        size_t current_head = head_.load(std::memory_order_relaxed);

        // Check if buffer is empty
        // Acquire: synchronize-with producer's release store to tail
        // Ensures we see all items producer has pushed
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false;  // Empty
        }

        // Read item from buffer (move for efficiency)
        item = std::move(buffer_[current_head]);

        // Publish new head position
        // Release: ensure item read happens-before this store
        // Producer will see freed slot when it reads this head value
        head_.store((current_head + 1) & (SIZE - 1), std::memory_order_release);

        return true;
    }

    /**
     * Query buffer status (approximate - no synchronization)
     * Useful for monitoring, not for correctness
     */
    bool empty() const {
        return head_.load(std::memory_order_relaxed) ==
               tail_.load(std::memory_order_relaxed);
    }

    /**
     * Approximate size (may be stale)
     */
    size_t size() const {
        size_t h = head_.load(std::memory_order_relaxed);
        size_t t = tail_.load(std::memory_order_relaxed);
        return (t >= h) ? (t - h) : (SIZE - h + t);
    }

    /**
     * Maximum capacity (one slot reserved for full/empty distinction)
     */
    constexpr size_t capacity() const {
        return SIZE - 1;
    }

private:
    // Cache-line alignment: prevents false sharing
    // Each atomic owns its own cache line (64 bytes on x86-64)

    // Consumer's read index
    alignas(64) std::atomic<size_t> head_;

    // Producer's write index
    alignas(64) std::atomic<size_t> tail_;

    // Data buffer (aligned to prevent false sharing with indices)
    alignas(64) std::array<T, SIZE> buffer_;
};

}  // namespace trading_ledger
