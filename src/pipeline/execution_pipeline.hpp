#pragma once

#include "../common/types.hpp"
#include "mpsc_queue.hpp"
#include <atomic>
#include <thread>

namespace exchange {

class ExecutionPipeline {
public:
    ExecutionPipeline();
    ~ExecutionPipeline();

    // Prevent copying
    ExecutionPipeline(const ExecutionPipeline&) = delete;
    ExecutionPipeline& operator=(const ExecutionPipeline&) = delete;

    void start();
    void stop();

    // Publish execution reports asynchronously using lock-free MPSC queue
    void publish_execution_report(const ExecutionReport& report);
    
    // Publish trade events asynchronously using lock-free MPSC queue
    void publish_trade(const Trade& trade);

private:
    void dispatch_loop();

    MPSCQueue<ExecutionReport> report_queue_;
    MPSCQueue<Trade> trade_queue_;
    std::atomic<bool> running_{false};
    std::thread dispatch_thread_;
};

} // namespace exchange
