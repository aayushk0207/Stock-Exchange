#pragma once

#include "../common/types.hpp"
#include "spsc_queue.hpp"
#include "wal_event.hpp"
#include <string>
#include <fstream>
#include <atomic>
#include <thread>

namespace exchange {

class WAL {
public:
    explicit WAL(const std::string& log_filepath);
    ~WAL();

    // Prevent copying
    WAL(const WAL&) = delete;
    WAL& operator=(const WAL&) = delete;

    void start();
    void stop();

    // High-performance asynchronous non-blocking log operations using SPSC queue
    void log_order(const Order& order);
    void log_cancel(const CancelRequest& req);
    void log_modify(const ModifyRequest& req);
    void log_fill(const Fill& fill);

private:
    void flush_loop();

    std::string filepath_;
    std::ofstream file_stream_;
    SPSCQueue<WALEntry> queue_;
    std::atomic<bool> running_{false};
    std::thread flush_thread_;
};

} // namespace exchange
