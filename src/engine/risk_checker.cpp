#include "risk_checker.hpp"
#include "../common/constants.hpp"

namespace exchange {

void RiskChecker::register_symbol(const Symbol& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    allowed_symbols_.insert(symbol);
}

bool RiskChecker::validate_order(const Order& order, std::string& reject_reason) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (allowed_symbols_.find(order.symbol) == allowed_symbols_.end()) {
        reject_reason = "Invalid Symbol";
        return false;
    }

    if (order.price <= 0 || order.price > constants::MAX_PRICE) {
        reject_reason = "Invalid Price Bounds";
        return false;
    }

    if (order.quantity <= 0 || order.quantity > constants::MAX_QUANTITY) {
        reject_reason = "Invalid Quantity Bounds";
        return false;
    }

    if (seen_order_ids_.find(order.order_id) != seen_order_ids_.end()) {
        reject_reason = "Duplicate Order ID";
        return false;
    }

    uint64_t order_value = static_cast<uint64_t>(order.price) * order.quantity;
    constexpr uint64_t MAX_ORDER_VALUE = 500000000;
    if (order_value > MAX_ORDER_VALUE) {
        reject_reason = "Order Value Exceeds Limit";
        return false;
    }

    seen_order_ids_.insert(order.order_id);
    return true;
}

void RiskChecker::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    allowed_symbols_.clear();
    seen_order_ids_.clear();
}

}
