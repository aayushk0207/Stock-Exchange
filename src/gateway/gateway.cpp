#include "gateway.hpp"
#include "../logger/logger.hpp"
#include "../threadpool/thread_pool.hpp"

namespace exchange {

Gateway::Gateway(ThreadPool& thread_pool)
    : thread_pool_(thread_pool) {}

Gateway::~Gateway() {
    stop();
}

void Gateway::start() {
    running_ = true;
    LOG_INFO("Gateway started.");
}

void Gateway::stop() {
    if (running_) {
        running_ = false;
        LOG_INFO("Gateway stopped.");
    }
}

void Gateway::submit_order(const Order& order) {
    if (!running_) {
        LOG_WARN("Gateway is not running. Order rejected: " + std::to_string(order.order_id));
        return;
    }
    LOG_INFO("Gateway received order " + std::to_string(order.order_id) + " for symbol " + order.symbol);
    // Future: enqueue task to thread pool or downstream queue
}

} // namespace exchange
