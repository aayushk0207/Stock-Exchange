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
    Order incoming_order = order; // Copy so we can modify remaining quantity

    if (incoming_order.side == Side::Buy) {
        auto asks_it = asks_.begin();

        while (incoming_order.remaining_quantity > 0 && asks_it != asks_.end()) {
            Price best_ask_price = asks_it->first;

            // Stop matching if limit order price is below the best ask
            if (incoming_order.type == OrderType::Limit && incoming_order.price < best_ask_price) {
                break;
            }

            std::list<Order>& queue = asks_it->second;

            while (incoming_order.remaining_quantity > 0 && !queue.empty()) {
                Order& resting_sell = queue.front();

                // Delegate only the execution match of the best pair to MatchingEngine
                MatchingEngine::matchOrders(incoming_order, resting_sell, symbol_, result);

                // Book state management
                if (resting_sell.remaining_quantity == 0) {
                    order_index_.remove(resting_sell.order_id);
                    queue.pop_front();
                }
            }

            // Remove empty price levels
            if (queue.empty()) {
                asks_it = asks_.erase(asks_it);
            } else {
                ++asks_it;
            }
        }

        // Handle remaining quantity
        if (incoming_order.remaining_quantity > 0) {
            if (incoming_order.type == OrderType::Limit) {
                bids_[incoming_order.price].push_back(incoming_order);
                auto position_it = --bids_[incoming_order.price].end();
                order_index_.insert(incoming_order.order_id, Side::Buy, incoming_order.price, position_it);

                // Receipt report if no matching occurred
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
                // Market order expired
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
    } else { // Sell Side
        auto bids_it = bids_.begin();

        while (incoming_order.remaining_quantity > 0 && bids_it != bids_.end()) {
            Price best_bid_price = bids_it->first;

            // Stop matching if limit order price is above the best bid
            if (incoming_order.type == OrderType::Limit && incoming_order.price > best_bid_price) {
                break;
            }

            std::list<Order>& queue = bids_it->second;

            while (incoming_order.remaining_quantity > 0 && !queue.empty()) {
                Order& resting_buy = queue.front();

                // Delegate only the execution match of the best pair to MatchingEngine
                MatchingEngine::matchOrders(resting_buy, incoming_order, symbol_, result);

                // Book state management
                if (resting_buy.remaining_quantity == 0) {
                    order_index_.remove(resting_buy.order_id);
                    queue.pop_front();
                }
            }

            // Remove empty price levels
            if (queue.empty()) {
                bids_it = bids_.erase(bids_it);
            } else {
                ++bids_it;
            }
        }

        // Handle remaining quantity
        if (incoming_order.remaining_quantity > 0) {
            if (incoming_order.type == OrderType::Limit) {
                asks_[incoming_order.price].push_back(incoming_order);
                auto position_it = --asks_[incoming_order.price].end();
                order_index_.insert(incoming_order.order_id, Side::Sell, incoming_order.price, position_it);

                // Receipt report if no matching occurred
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
                // Market order expired
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
        auto it = bids_.find(entry.price_level);
        if (it != bids_.end()) {
            it->second.erase(entry.position);
            if (it->second.empty()) {
                bids_.erase(it);
            }
        }
    } else {
        auto it = asks_.find(entry.price_level);
        if (it != asks_.end()) {
            it->second.erase(entry.position);
            if (it->second.empty()) {
                asks_.erase(it);
            }
        }
    }

    // Generate Cancel execution report
    ExecutionReport cancel_report;
    cancel_report.order_id = order_id;
    cancel_report.symbol = symbol_;
    cancel_report.side = entry.side;
    cancel_report.status = OrderStatus::Cancelled;
    cancel_report.price = entry.price_level;
    cancel_report.timestamp = time_utils::get_current_time_ns();
    cancel_report.client_id = cid;
    result.execution_reports.push_back(cancel_report);

    // Remove from index
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

    // Get a copy of the existing order from its queue location
    Order existing_order = *entry.position;

    // Remove it from its current price level queue (cancelling original priority)
    if (entry.side == Side::Buy) {
        auto it = bids_.find(entry.price_level);
        if (it != bids_.end()) {
            it->second.erase(entry.position);
            if (it->second.empty()) {
                bids_.erase(it);
            }
        }
    } else {
        auto it = asks_.find(entry.price_level);
        if (it != asks_.end()) {
            it->second.erase(entry.position);
            if (it->second.empty()) {
                asks_.erase(it);
            }
        }
    }
    order_index_.remove(request.order_id);

    // Update the order with modified parameters
    existing_order.price = request.price;
    existing_order.quantity = request.quantity;
    existing_order.remaining_quantity = request.quantity; // Resets remaining fill size
    existing_order.timestamp = request.timestamp;

    // Re-submit it as a new order using the helper to avoid mutex deadlocks
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

/*
void OrderBook::clearBook() {
    std::lock_guard<std::mutex> lock(book_mutex_);
    bids_.clear();
    asks_.clear();
    order_index_.clear();
}
*/

Price OrderBook::bestBid() const {
    std::lock_guard<std::mutex> lock(book_mutex_);
    if (bids_.empty()) {
        return 0;
    }
    return bids_.begin()->first;
}

Price OrderBook::bestAsk() const {
    std::lock_guard<std::mutex> lock(book_mutex_);
    if (asks_.empty()) {
        return 0;
    }
    return asks_.begin()->first;
}

/*
size_t OrderBook::totalOrders() const {
    std::lock_guard<std::mutex> lock(book_mutex_);
    return order_index_.size();
}

size_t OrderBook::totalBidLevels() const {
    std::lock_guard<std::mutex> lock(book_mutex_);
    return bids_.size();
}

size_t OrderBook::totalAskLevels() const {
    std::lock_guard<std::mutex> lock(book_mutex_);
    return asks_.size();
}

bool OrderBook::isEmpty() const {
    std::lock_guard<std::mutex> lock(book_mutex_);
    return bids_.empty() && asks_.empty();
}
*/

void OrderBook::print_book() const {
    std::lock_guard<std::mutex> lock(book_mutex_);
    LOG_INFO("=== Order Book: " + symbol_ + " ===");
    LOG_INFO("  --- ASKS (Sells) ---");
    for (auto it = asks_.rbegin(); it != asks_.rend(); ++it) {
        uint32_t qty_at_price = 0;
        for (const auto& ord : it->second) {
            qty_at_price += ord.remaining_quantity;
        }
        LOG_INFO("    Price: " + std::to_string(it->first) + " | Qty: " + std::to_string(qty_at_price) + " (" + std::to_string(it->second.size()) + " orders)");
    }
    LOG_INFO("  --------------------");
    LOG_INFO("  --- BIDS (Buys) ---");
    for (auto it = bids_.begin(); it != bids_.end(); ++it) {
        uint32_t qty_at_price = 0;
        for (const auto& ord : it->second) {
            qty_at_price += ord.remaining_quantity;
        }
        LOG_INFO("    Price: " + std::to_string(it->first) + " | Qty: " + std::to_string(qty_at_price) + " (" + std::to_string(it->second.size()) + " orders)");
    }
    LOG_INFO("===========================");
}

} // namespace exchange
