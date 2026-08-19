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

    bool enqueue(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        while (queue_.size() >= max_capacity_ && !shutdown_) {
            cond_push_.wait(lock);
        }

        if (shutdown_) {
            return false;
        }

        queue_.push(std::move(item));
        cond_pop_.notify_one();
        return true;
    }

    std::optional<T> dequeue() {
        std::unique_lock<std::mutex> lock(mutex_);
        while (queue_.empty() && !shutdown_) {
            cond_pop_.wait(lock);
        }

        if (queue_.empty() && shutdown_) {
            return std::nullopt;
        }

        T item = std::move(queue_.front());
        queue_.pop();
        cond_push_.notify_one();
        return item;
    }

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

}
