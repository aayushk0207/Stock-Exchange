#pragma once

#include "../common/types.hpp"
#include <list>
#include <unordered_map>
#include <optional>
#include <mutex>

namespace exchange {

// Represents an entry in the order index.
// Stores the metadata needed to quickly find, access, or remove an order from the book queues.
struct OrderIndexEntry {
    Side side;
    Price price_level;
    std::list<Order>::iterator position;
};

// Thread-safe index supporting O(1) lookup/deletion of resting orders by OrderID.
class OrderIndex {
public:
    OrderIndex() = default;
    ~OrderIndex() = default;

    // Prevent copying
    OrderIndex(const OrderIndex&) = delete;
    OrderIndex& operator=(const OrderIndex&) = delete;

    // Insert or update an index entry
    void insert(OrderID order_id, Side side, Price price, std::list<Order>::iterator position) {
        std::lock_guard<std::mutex> lock(mutex_);
        index_[order_id] = OrderIndexEntry{side, price, position};
    }

    // Retrieve an entry if it exists, otherwise return std::nullopt
    std::optional<OrderIndexEntry> get(OrderID order_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = index_.find(order_id);
        if (it == index_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    // Remove an entry by OrderID. Returns true if removed, false if not found.
    bool remove(OrderID order_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        return index_.erase(order_id) > 0;
    }

    // Clear all index entries
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        index_.clear();
    }

    // Get current size of index
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return index_.size();
    }

private:
    std::unordered_map<OrderID, OrderIndexEntry> index_;
    mutable std::mutex mutex_;
};

} // namespace exchange
