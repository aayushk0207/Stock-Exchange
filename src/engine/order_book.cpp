#include "order_book.hpp"
#include "matching_engine.hpp"
#include "../logger/logger.hpp"
#include "../common/time_utils.hpp"

namespace exchange {

OrderBook::OrderBook(Symbol symbol)
    : symbol_(std::move(symbol)) {}

MatchResult OrderBook::submitOrder(const Order& order) {
    std::lock_guard<std::mutex> lock(book_mutex_);
    MatchResult result;
    submitOrderInternal(order, result);
    return result;
}

void OrderBook::submitOrderInternal(const Order& order, MatchResult& result) {
    Order incoming_order = order;

    if (incoming_order.side == Side::Buy) {
        auto asks_it = sell_orders_.begin();

        while (incoming_order.remaining_quantity > 0 && asks_it != sell_orders_.end()) {
            Price best_ask_price = asks_it->first;

            if (incoming_order.type == OrderType::Limit && incoming_order.price < best_ask_price) {
                break;
            }

            std::list<Order>& queue = asks_it->second;

            while (incoming_order.remaining_quantity > 0 && !queue.empty()) {
                Order& resting_sell = queue.front();
                MatchingEngine::matchOrders(incoming_order, resting_sell, symbol_, result);

                if (resting_sell.remaining_quantity == 0) {
                    order_index_.remove(resting_sell.order_id);
                    queue.pop_front();
                }
            }

            if (queue.empty()) {
                asks_it = sell_orders_.erase(asks_it);
            } else {
                ++asks_it;
            }
        }

        if (incoming_order.remaining_quantity > 0) {
            if (incoming_order.type == OrderType::Limit) {
                buy_orders_[incoming_order.price].push_back(incoming_order);
                auto position_it = --buy_orders_[incoming_order.price].end();
                order_index_.insert(incoming_order.order_id, Side::Buy, incoming_order.price, position_it);

                if (incoming_order.remaining_quantity == incoming_order.quantity) {
                    ExecutionReport new_report;
                    new_report.order_id = incoming_order.order_id;
                    new_report.symbol = incoming_order.symbol;
                    new_report.side = incoming_order.side;
                    new_report.status = OrderStatus::New;
                    new_report.price = incoming_order.price;
                    new_report.quantity = incoming_order.quantity;
                    new_report.remaining_quantity = incoming_order.remaining_quantity;
                    new_report.timestamp = time_utils::get_current_time_ns();
                    new_report.client_id = incoming_order.client_id;
                    result.execution_reports.push_back(new_report);
                }
            } else {
                ExecutionReport cancel_report;
                cancel_report.order_id = incoming_order.order_id;
                cancel_report.symbol = incoming_order.symbol;
                cancel_report.side = incoming_order.side;
                cancel_report.status = OrderStatus::Cancelled;
                cancel_report.price = incoming_order.price;
                cancel_report.quantity = incoming_order.quantity;
                cancel_report.remaining_quantity = incoming_order.remaining_quantity;
                cancel_report.reject_reason = "Market Order Expired / Unfilled Qty";
                cancel_report.timestamp = time_utils::get_current_time_ns();
                cancel_report.client_id = incoming_order.client_id;
                result.execution_reports.push_back(cancel_report);
            }
        }
    } else {
        auto bids_it = buy_orders_.begin();

        while (incoming_order.remaining_quantity > 0 && bids_it != buy_orders_.end()) {
            Price best_bid_price = bids_it->first;

            if (incoming_order.type == OrderType::Limit && incoming_order.price > best_bid_price) {
                break;
            }

            std::list<Order>& queue = bids_it->second;

            while (incoming_order.remaining_quantity > 0 && !queue.empty()) {
                Order& resting_buy = queue.front();
                MatchingEngine::matchOrders(resting_buy, incoming_order, symbol_, result);

                if (resting_buy.remaining_quantity == 0) {
                    order_index_.remove(resting_buy.order_id);
                    queue.pop_front();
                }
            }

            if (queue.empty()) {
                bids_it = buy_orders_.erase(bids_it);
            } else {
                ++bids_it;
            }
        }

        if (incoming_order.remaining_quantity > 0) {
            if (incoming_order.type == OrderType::Limit) {
                sell_orders_[incoming_order.price].push_back(incoming_order);
                auto position_it = --sell_orders_[incoming_order.price].end();
                order_index_.insert(incoming_order.order_id, Side::Sell, incoming_order.price, position_it);

                if (incoming_order.remaining_quantity == incoming_order.quantity) {
                    ExecutionReport new_report;
                    new_report.order_id = incoming_order.order_id;
                    new_report.symbol = incoming_order.symbol;
                    new_report.side = incoming_order.side;
                    new_report.status = OrderStatus::New;
                    new_report.price = incoming_order.price;
                    new_report.quantity = incoming_order.quantity;
                    new_report.remaining_quantity = incoming_order.remaining_quantity;
                    new_report.timestamp = time_utils::get_current_time_ns();
                    new_report.client_id = incoming_order.client_id;
                    result.execution_reports.push_back(new_report);
                }
            } else {
                ExecutionReport cancel_report;
                cancel_report.order_id = incoming_order.order_id;
                cancel_report.symbol = incoming_order.symbol;
                cancel_report.side = incoming_order.side;
                cancel_report.status = OrderStatus::Cancelled;
                cancel_report.price = incoming_order.price;
                cancel_report.quantity = incoming_order.quantity;
                cancel_report.remaining_quantity = incoming_order.remaining_quantity;
                cancel_report.reject_reason = "Market Order Expired / Unfilled Qty";
                cancel_report.timestamp = time_utils::get_current_time_ns();
                cancel_report.client_id = incoming_order.client_id;
                result.execution_reports.push_back(cancel_report);
            }
        }
    }
}

bool OrderBook::cancelOrder(OrderID order_id, MatchResult& result) {
    std::lock_guard<std::mutex> lock(book_mutex_);
    auto opt = order_index_.get(order_id);
    if (!opt.has_value()) {
        return false;
    }

    auto& entry = opt.value();
    uint32_t cid = entry.position->client_id;

    if (entry.side == Side::Buy) {
        auto it = buy_orders_.find(entry.price_level);
        if (it != buy_orders_.end()) {
            it->second.erase(entry.position);
            if (it->second.empty()) {
                buy_orders_.erase(it);
            }
        }
    } else {
        auto it = sell_orders_.find(entry.price_level);
        if (it != sell_orders_.end()) {
            it->second.erase(entry.position);
            if (it->second.empty()) {
                sell_orders_.erase(it);
            }
        }
    }

    ExecutionReport cancel_report;
    cancel_report.order_id = order_id;
    cancel_report.symbol = symbol_;
    cancel_report.side = entry.side;
    cancel_report.status = OrderStatus::Cancelled;
    cancel_report.price = entry.price_level;
    cancel_report.timestamp = time_utils::get_current_time_ns();
    cancel_report.client_id = cid;
    result.execution_reports.push_back(cancel_report);

    order_index_.remove(order_id);
    return true;
}

bool OrderBook::modifyOrder(const ModifyRequest& request, MatchResult& result) {
    std::lock_guard<std::mutex> lock(book_mutex_);
    auto opt = order_index_.get(request.order_id);
    if (!opt.has_value()) {
        return false;
    }

    auto& entry = opt.value();
    Order existing_order = *entry.position;

    if (entry.side == Side::Buy) {
        auto it = buy_orders_.find(entry.price_level);
        if (it != buy_orders_.end()) {
            it->second.erase(entry.position);
            if (it->second.empty()) {
                buy_orders_.erase(it);
            }
        }
    } else {
        auto it = sell_orders_.find(entry.price_level);
        if (it != sell_orders_.end()) {
            it->second.erase(entry.position);
            if (it->second.empty()) {
                sell_orders_.erase(it);
            }
        }
    }
    order_index_.remove(request.order_id);

    existing_order.price = request.price;
    existing_order.quantity = request.quantity;
    existing_order.remaining_quantity = request.quantity;
    existing_order.timestamp = request.timestamp;

    submitOrderInternal(existing_order, result);
    return true;
}

std::optional<Order> OrderBook::queryOrder(OrderID order_id) const {
    std::lock_guard<std::mutex> lock(book_mutex_);
    auto opt = order_index_.get(order_id);
    if (!opt.has_value()) {
        return std::nullopt;
    }
    return *(opt.value().position);
}

Price OrderBook::bestBid() const {
    std::lock_guard<std::mutex> lock(book_mutex_);
    if (buy_orders_.empty()) {
        return 0;
    }
    return buy_orders_.begin()->first;
}

Price OrderBook::bestAsk() const {
    std::lock_guard<std::mutex> lock(book_mutex_);
    if (sell_orders_.empty()) {
        return 0;
    }
    return sell_orders_.begin()->first;
}

void OrderBook::print_book() const {
    std::lock_guard<std::mutex> lock(book_mutex_);
    LOG_INFO("Order Book: " + symbol_);
    LOG_INFO("  ASKS:");
    for (auto it = sell_orders_.rbegin(); it != sell_orders_.rend(); ++it) {
        uint32_t qty_at_price = 0;
        for (const auto& ord : it->second) {
            qty_at_price += ord.remaining_quantity;
        }
        LOG_INFO("    Price: " + std::to_string(it->first) + " | Qty: " + std::to_string(qty_at_price) + " (" + std::to_string(it->second.size()) + " orders)");
    }
    LOG_INFO("  BIDS:");
    for (auto it = buy_orders_.begin(); it != buy_orders_.end(); ++it) {
        uint32_t qty_at_price = 0;
        for (const auto& ord : it->second) {
            qty_at_price += ord.remaining_quantity;
        }
        LOG_INFO("    Price: " + std::to_string(it->first) + " | Qty: " + std::to_string(qty_at_price) + " (" + std::to_string(it->second.size()) + " orders)");
    }
}

}
