#include "wal.hpp"
#include "../logger/logger.hpp"

namespace exchange {

WAL::WAL(const std::string& log_filepath)
    : filepath_(log_filepath) {}

WAL::~WAL() {
    stop();
}

void WAL::start() {
    if (running_) return;

    // Open file in append binary mode
    file_stream_.open(filepath_, std::ios::binary | std::ios::out | std::ios::app);
    if (!file_stream_.is_open()) {
        LOG_ERROR("Failed to open WAL file: " + filepath_);
        return;
    }

    running_ = true;
    flush_thread_ = std::thread(&WAL::flush_loop, this);
    LOG_INFO("WAL pipeline started. Log path: " + filepath_);
}

void WAL::stop() {
    if (running_.exchange(false)) {
        queue_.close();
        if (flush_thread_.joinable()) {
            flush_thread_.join();
        }

        if (file_stream_.is_open()) {
            file_stream_.close();
        }
        LOG_INFO("WAL pipeline stopped.");
    }
}

void WAL::log_order(const Order& order) {
    if (!running_) return;

    WALEntry entry;
    entry.type = static_cast<uint8_t>(WALEventType::Submit);
    entry.data.order = BinarySerializer::to_net(order);

    // High performance push to lock-free SPSC queue
    while (running_ && !queue_.write(entry)) {
        std::this_thread::yield(); // Block if full (rare for configured size)
    }
}

void WAL::log_cancel(const CancelRequest& req) {
    if (!running_) return;

    WALEntry entry;
    entry.type = static_cast<uint8_t>(WALEventType::Cancel);
    entry.data.cancel.order_id = req.order_id;
    BinarySerializer::safe_strncpy(entry.data.cancel.symbol, req.symbol, sizeof(entry.data.cancel.symbol));
    entry.data.cancel.timestamp = req.timestamp;

    while (running_ && !queue_.write(entry)) {
        std::this_thread::yield();
    }
}

void WAL::log_modify(const ModifyRequest& req) {
    if (!running_) return;

    WALEntry entry;
    entry.type = static_cast<uint8_t>(WALEventType::Modify);
    entry.data.modify.order_id = req.order_id;
    BinarySerializer::safe_strncpy(entry.data.modify.symbol, req.symbol, sizeof(entry.data.modify.symbol));
    entry.data.modify.price = req.price;
    entry.data.modify.quantity = req.quantity;
    entry.data.modify.timestamp = req.timestamp;

    while (running_ && !queue_.write(entry)) {
        std::this_thread::yield();
    }
}

void WAL::log_fill(const Fill& fill) {
    if (!running_) return;

    WALEntry entry;
    entry.type = static_cast<uint8_t>(WALEventType::Fill);
    entry.data.fill.trade_id = fill.trade_id;
    entry.data.fill.buy_order_id = fill.buy_order_id;
    entry.data.fill.sell_order_id = fill.sell_order_id;
    BinarySerializer::safe_strncpy(entry.data.fill.symbol, fill.symbol, sizeof(entry.data.fill.symbol));
    entry.data.fill.price = fill.price;
    entry.data.fill.quantity = fill.quantity;
    entry.data.fill.timestamp = fill.timestamp;

    while (running_ && !queue_.write(entry)) {
        std::this_thread::yield();
    }
}

void WAL::flush_loop() {
    while (running_ || !queue_.empty()) {
        auto entry_opt = queue_.read();
        if (entry_opt.has_value()) {
            file_stream_.write(reinterpret_cast<const char*>(&entry_opt.value()), sizeof(WALEntry));
            file_stream_.flush(); // Ensure durability on disk immediately
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(100)); // Avoid spinning CPU
        }
    }
}

} // namespace exchange
