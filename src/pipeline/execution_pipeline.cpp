#include "execution_pipeline.hpp"
#include "../logger/logger.hpp"

namespace exchange {

ExecutionPipeline::ExecutionPipeline() = default;

ExecutionPipeline::~ExecutionPipeline() {
    stop();
}

void ExecutionPipeline::start() {
    running_ = true;
    LOG_INFO("Execution pipeline started.");
    // In future phases, we will spawn the dispatch thread here
}

void ExecutionPipeline::stop() {
    if (running_) {
        running_ = false;
        LOG_INFO("Execution pipeline stopped.");
        // In future phases, we will join the dispatch thread here
    }
}

void ExecutionPipeline::publish_execution_report(const ExecutionReport& report) {
    LOG_INFO("ExecutionPipeline: Publishing execution report for order ID: " + std::to_string(report.order_id));
    // In future phases, write report to report_queue_ and notify dispatch_thread_
}

void ExecutionPipeline::publish_trade(const Trade& trade) {
    LOG_INFO("ExecutionPipeline: Publishing trade for symbol: " + trade.symbol + ", quantity: " + std::to_string(trade.quantity));
    // In future phases, write trade to trade_queue_ and notify dispatch_thread_
}

void ExecutionPipeline::dispatch_loop() {
    // Stub for background thread dispatching execution events to client gateway or logs
}

} // namespace exchange
