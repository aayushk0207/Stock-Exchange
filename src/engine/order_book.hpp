#pragma once

#include "order_index.hpp"
#include <map>
#include <list>
#include <mutex>
#include <functional>
#include <optional>

namespace exchange {

class OrderBook {
public:
    OrderBook(Symbol symbol);
    ~OrderBook() = default;

    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;

    const Symbol& get_symbol() const { return symbol_; }

    MatchResult submitOrder(const Order& order, std::function<void()> log_func = nullptr);
    bool cancelOrder(OrderID order_id, MatchResult& result, std::function<void()> log_func = nullptr);
    bool modifyOrder(const ModifyRequest& request, MatchResult& result, std::function<void()> log_func = nullptr);
    std::optional<Order> queryOrder(OrderID order_id) const;
    Price bestBid() const;
    Price bestAsk() const;
    void print_book() const;

private:
    friend class MatchingEngine;
    void submitOrderInternal(const Order& order, MatchResult& result);

    Symbol symbol_;
    std::map<Price, std::list<Order>, std::greater<Price>> buy_orders_;
    std::map<Price, std::list<Order>> sell_orders_;
    OrderIndex order_index_;
    mutable std::mutex book_mutex_;
};

}
