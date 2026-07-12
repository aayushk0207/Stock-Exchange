#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include "../common/constants.hpp"

namespace exchange {

template <typename T>
class SafeQueue {
public:
    SafeQueue(size_t max_capacity = constants::MAX_QUEUE_CAPACITY)
        : max_capacity_(max_capacity), shutdown_(false) {}

    ~SafeQueue() {
        close();
    }

    // Bounded thread-safe enqueue. Blocks if queue is full.
    // Returns false if queue was closed during wait or is already closed.
    bool enqueue(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_push_.wait(lock, [this]() { return queue_.size() < max_capacity_ || shutdown_; });

        if (shutdown_) {
            return false;
        }

        queue_.push(std::move(item));
        cond_pop_.notify_one();
        return true;
    }

    // Blocking dequeue. Blocks if queue is empty.
    // Returns std::nullopt if queue is closed and empty.
    std::optional<T> dequeue() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_pop_.wait(lock, [this]() { return !queue_.empty() || shutdown_; });

        if (queue_.empty() && shutdown_) {
            return std::nullopt;
        }

        T item = std::move(queue_.front());
        queue_.pop();
        cond_push_.notify_one();
        return item;
    }

    // Close the queue and wake up all waiting threads
    void close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (shutdown_) return;
            shutdown_ = true;
        }
        cond_pop_.notify_all();
        cond_push_.notify_all();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    std::queue<T> queue_;
    size_t max_capacity_;
    bool shutdown_;
    mutable std::mutex mutex_;
    std::condition_variable cond_pop_;
    std::condition_variable cond_push_;
};

} // namespace exchange
