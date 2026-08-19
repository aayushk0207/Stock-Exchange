#pragma once

#include "../common/types.hpp"
#include <string>
#include <unordered_set>
#include <mutex>

namespace exchange {

class RiskChecker {
public:
    static RiskChecker& get_instance() {
        static RiskChecker instance;
        return instance;
    }

    RiskChecker(const RiskChecker&) = delete;
    RiskChecker& operator=(const RiskChecker&) = delete;

    void register_symbol(const Symbol& symbol);
    bool validate_order(const Order& order, std::string& reject_reason);
    void reset();

private:
    RiskChecker() = default;
    ~RiskChecker() = default;

    std::unordered_set<Symbol> allowed_symbols_;
    std::unordered_set<OrderID> seen_order_ids_;
    mutable std::mutex mutex_;
};

}
