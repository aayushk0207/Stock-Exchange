#pragma once

#include <vector>
#include <thread>
#include <atomic>
#include "safe_queue.hpp"
#include "task.hpp"

namespace exchange {

class BookRegistry;
class WAL;
class ExecutionPipeline;

class ThreadPool {
public:
    ThreadPool(BookRegistry& registry, size_t num_threads = constants::DEFAULT_THREAD_POOL_SIZE, WAL* wal = nullptr, ExecutionPipeline* pipeline = nullptr);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void start();
    void shutdown();

    bool submit(Task task);

private:
    void worker_loop();

    BookRegistry& registry_;
    size_t num_threads_;
    WAL* wal_ = nullptr;
    ExecutionPipeline* pipeline_ = nullptr;
    std::vector<std::thread> workers_;
    SafeQueue<Task> task_queue_;
    std::atomic<bool> stop_{false};
};

}
