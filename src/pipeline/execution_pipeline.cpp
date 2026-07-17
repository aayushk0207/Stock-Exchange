#include "execution_pipeline.hpp"
#include "../logger/logger.hpp"
#include "../gateway/connection_manager.hpp"
#include "../gateway/session.hpp"

namespace exchange {

ExecutionPipeline::ExecutionPipeline() = default;

ExecutionPipeline::~ExecutionPipeline() {
    stop();
}

void ExecutionPipeline::start() {
    if (running_) return;
    running_ = true;
    dispatch_thread_ = std::thread(&ExecutionPipeline::dispatch_loop, this);
    LOG_INFO("Execution pipeline started.");
}

void ExecutionPipeline::stop() {
    if (running_.exchange(false)) {
        report_queue_.close();
        trade_queue_.close();
        if (dispatch_thread_.joinable()) {
            dispatch_thread_.join();
        }
        LOG_INFO("Execution pipeline stopped.");
    }
}

void ExecutionPipeline::publish_execution_report(const ExecutionReport& report) {
    if (!running_) return;
    while (running_ && !report_queue_.write(report)) {
        std::this_thread::yield();
    }
}

void ExecutionPipeline::publish_trade(const Trade& trade) {
    if (!running_) return;
    while (running_ && !trade_queue_.write(trade)) {
        std::this_thread::yield();
    }
}

void ExecutionPipeline::dispatch_loop() {
    while (running_ || !report_queue_.empty() || !trade_queue_.empty()) {
        bool idle = true;
        
        auto report_opt = report_queue_.read();
        if (report_opt.has_value()) {
            const auto& report = report_opt.value();
            auto session = ConnectionManager::get_instance().get_session(report.client_id);
            if (session) {
                LOG_INFO("[DISPATCH] Sending execution report to Client " + std::to_string(report.client_id));
                session->send_execution_report(report);
            }
            idle = false;
        }

        auto trade_opt = trade_queue_.read();
        if (trade_opt.has_value()) {
            // Asynchronously consumed trade event.
            idle = false;
        }

        if (idle) {
            std::this_thread::sleep_for(std::chrono::microseconds(100)); // Avoid spinning CPU when idle
        }
    }
}

} // namespace exchange
