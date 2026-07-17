#pragma once

#include "order_index.hpp"
#include <map>
#include <list>
#include <mutex>
#include <optional>

namespace exchange {

class OrderBook {
public:
    explicit OrderBook(Symbol symbol);
    ~OrderBook() = default;

    // Prevent copying
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;

    const Symbol& get_symbol() const { return symbol_; }

    // Core requirements
    MatchResult submitOrder(const Order& order);
    bool cancelOrder(OrderID order_id, MatchResult& result);
    bool modifyOrder(const ModifyRequest& request, MatchResult& result);
    std::optional<Order> queryOrder(OrderID order_id) const;
    // void clearBook();

    // Statistics getters
    Price bestBid() const;
    Price bestAsk() const;
    // size_t totalOrders() const;
    // size_t totalBidLevels() const;
    // size_t totalAskLevels() const;
    // bool isEmpty() const;
    void print_book() const;

private:
    friend class MatchingEngine;
    void submitOrderInternal(const Order& order, MatchResult& result);

    Symbol symbol_;

    // Standard order book structure:
    // Bids sorted in descending order (highest price first)
    std::map<Price, std::list<Order>, std::greater<Price>> bids_;
    // Asks sorted in ascending order (lowest price first)
    std::map<Price, std::list<Order>> asks_;

    // O(1) order index for fast cancellations and modifications
    OrderIndex order_index_;

    mutable std::mutex book_mutex_;
};

} // namespace exchange
