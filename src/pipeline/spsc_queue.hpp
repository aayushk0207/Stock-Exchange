#pragma once

#include <atomic>
#include <vector>
#include <optional>
#include <cstddef>

namespace exchange {

// A simple, clean, lock-free Single Producer Single Consumer (SPSC) queue.
// This is used for async pipelines like WAL logging and Execution Reports.
template <typename T, size_t Capacity = 65536>
class SPSCQueue {
public:
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

    SPSCQueue() : head_(0), tail_(0) {
        ring_buffer_.resize(Capacity);
    }

    ~SPSCQueue() = default;

    // Prevent copying
    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;

    // Push item into the queue. Returns false if full. Called ONLY by producer.
    bool write(const T& item) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t current_head = head_.load(std::memory_order_acquire);

        if ((current_tail - current_head) >= Capacity) {
            return false; // Queue is full
        }

        ring_buffer_[current_tail & (Capacity - 1)] = item;
        tail_.store(current_tail + 1, std::memory_order_release);
        return true;
    }

    bool write(T&& item) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t current_head = head_.load(std::memory_order_acquire);

        if ((current_tail - current_head) >= Capacity) {
            return false; // Queue is full
        }

        ring_buffer_[current_tail & (Capacity - 1)] = std::move(item);
        tail_.store(current_tail + 1, std::memory_order_release);
        return true;
    }

    // Read item from the queue. Returns std::nullopt if empty. Called ONLY by consumer.
    std::optional<T> read() {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        const size_t current_tail = tail_.load(std::memory_order_acquire);

        if (current_head == current_tail) {
            return std::nullopt; // Queue is empty
        }

        T item = std::move(ring_buffer_[current_head & (Capacity - 1)]);
        head_.store(current_head + 1, std::memory_order_release);
        return item;
    }

    bool empty() const {
        return head_.load(std::memory_order_relaxed) == tail_.load(std::memory_order_relaxed);
    }

    size_t size() const {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        return (current_tail > current_head) ? (current_tail - current_head) : 0;
    }

private:
    std::vector<T> ring_buffer_;
    // Align atomic indices to separate cache lines to avoid false sharing
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
};

} // namespace exchange
