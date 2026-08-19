#pragma once

#include "../common/types.hpp"
#include <list>
#include <unordered_map>
#include <optional>
#include <mutex>

namespace exchange {

struct OrderIndexEntry {
    Side side;
    Price price_level;
    std::list<Order>::iterator position;
};

class OrderIndex {
public:
    OrderIndex() = default;
    ~OrderIndex() = default;

    OrderIndex(const OrderIndex&) = delete;
    OrderIndex& operator=(const OrderIndex&) = delete;

    void insert(OrderID order_id, Side side, Price price, std::list<Order>::iterator position) {
        std::lock_guard<std::mutex> lock(mutex_);
        index_[order_id] = OrderIndexEntry{side, price, position};
    }

    std::optional<OrderIndexEntry> get(OrderID order_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = index_.find(order_id);
        if (it == index_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    bool remove(OrderID order_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        return index_.erase(order_id) > 0;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        index_.clear();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return index_.size();
    }

private:
    std::unordered_map<OrderID, OrderIndexEntry> index_;
    mutable std::mutex mutex_;
};

}
