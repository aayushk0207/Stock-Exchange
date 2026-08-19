#pragma once

#include <atomic>
#include <cstdint>

namespace exchange {

class TradeIDGenerator {
public:
    static TradeIDGenerator& get_instance() {
        static TradeIDGenerator instance;
        return instance;
    }

    TradeIDGenerator(const TradeIDGenerator&) = delete;
    TradeIDGenerator& operator=(const TradeIDGenerator&) = delete;

    void reset(uint64_t start_id = 0) {
        counter_.store(start_id, std::memory_order_relaxed);
    }

    uint64_t next_id() {
        return counter_.fetch_add(1, std::memory_order_relaxed);
    }

private:
    TradeIDGenerator() : counter_(1) {}
    ~TradeIDGenerator() = default;

    std::atomic<uint64_t> counter_;
};

}
