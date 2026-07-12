#include "matching_engine.hpp"
#include "order_book.hpp"
#include "../registry/book_registry.hpp"
#include "../pipeline/wal.hpp"
#include "../pipeline/execution_pipeline.hpp"
#include "../logger/logger.hpp"
#include "../common/time_utils.hpp"
#include "../common/trade_id_generator.hpp"

namespace exchange {

MatchingEngine::MatchingEngine(BookRegistry& registry, ExecutionPipeline& pipeline, WAL& wal)
    : registry_(registry), pipeline_(pipeline), wal_(wal) {}

void MatchingEngine::process_order(const Order& order) {
    LOG_INFO("Processing incoming order: ID=" + std::to_string(order.order_id) +
             ", Symbol=" + order.symbol + ", Qty=" + std::to_string(order.quantity) +
             ", Price=" + std::to_string(order.price));

    // 1. Log to WAL for durability
    wal_.log_order(order);

    // 2. Fetch order book
    OrderBook* book = registry_.get_order_book(order.symbol);
    if (!book) {
        LOG_ERROR("Symbol " + order.symbol + " not found in registry.");
        ExecutionReport reject_report;
        reject_report.order_id = order.order_id;
        reject_report.symbol = order.symbol;
        reject_report.side = order.side;
        reject_report.status = OrderStatus::Rejected;
        reject_report.reject_reason = "Symbol Not Found";
        reject_report.timestamp = time_utils::get_current_time_ns();
        pipeline_.publish_execution_report(reject_report);
        return;
    }

    // 3. Match the order (delegated to OrderBook which controls the loop)
    MatchResult result = book->submitOrder(order);

    // 4. Publish resulting trade fills & execution reports
    for (const auto& fill : result.fills) {
        wal_.log_fill(fill);
        pipeline_.publish_trade(Trade{fill.trade_id, fill.symbol, fill.price, fill.quantity, fill.timestamp});
    }

    for (const auto& report : result.execution_reports) {
        pipeline_.publish_execution_report(report);
    }
}

void MatchingEngine::process_cancel(OrderID order_id, const Symbol& symbol) {
    LOG_INFO("Processing cancel request for order: ID=" + std::to_string(order_id) + ", Symbol=" + symbol);

    // 1. Log cancel to WAL
    CancelRequest req{order_id, symbol, time_utils::get_current_time_ns()};
    wal_.log_cancel(req);

    // 2. Get OrderBook
    OrderBook* book = registry_.get_order_book(symbol);
    if (!book) {
        LOG_ERROR("Symbol " + symbol + " not found for cancel.");
        return;
    }

    // 3. Cancel the order
    MatchResult result;
    if (book->cancelOrder(order_id, result)) {
        for (const auto& report : result.execution_reports) {
            pipeline_.publish_execution_report(report);
        }
    } else {
        LOG_WARN("Cancel failed (order not found): ID=" + std::to_string(order_id));
    }
}

void MatchingEngine::process_modify(const ModifyRequest& request) {
    LOG_INFO("Processing modify request for order: ID=" + std::to_string(request.order_id) +
             ", Symbol=" + request.symbol + ", NewQty=" + std::to_string(request.quantity) +
             ", NewPrice=" + std::to_string(request.price));

    // 1. Log modify to WAL
    wal_.log_modify(request);

    // 2. Get OrderBook
    OrderBook* book = registry_.get_order_book(request.symbol);
    if (!book) {
        LOG_ERROR("Symbol " + request.symbol + " not found for modify.");
        return;
    }

    // 3. Modify the order (removes and re-submits to lose priority)
    MatchResult result;
    if (book->modifyOrder(request, result)) {
        for (const auto& fill : result.fills) {
            wal_.log_fill(fill);
            pipeline_.publish_trade(Trade{fill.trade_id, fill.symbol, fill.price, fill.quantity, fill.timestamp});
        }
        for (const auto& report : result.execution_reports) {
            pipeline_.publish_execution_report(report);
        }
    } else {
        LOG_WARN("Modify failed (order not found): ID=" + std::to_string(request.order_id));
    }
}

void MatchingEngine::matchOrders(Order& buy_order, Order& sell_order, const Symbol& symbol, MatchResult& result) {
    Quantity match_qty = std::min(buy_order.remaining_quantity, sell_order.remaining_quantity);
    if (match_qty == 0) {
        return;
    }

    uint64_t trade_id = TradeIDGenerator::get_instance().next_id();
    Timestamp now = time_utils::get_current_time_ns();

    // The price is set by the resting order (which has the earlier timestamp)
    Price exec_price = (sell_order.timestamp < buy_order.timestamp) ? sell_order.price : buy_order.price;

    LOG_INFO("[MATCH]\nBUY Order " + std::to_string(buy_order.order_id) + " matched with SELL Order " + std::to_string(sell_order.order_id) +
             "\nPrice: " + std::to_string(exec_price) +
             "\nQuantity: " + std::to_string(match_qty));

    // 1. Generate Fill
    Fill fill{trade_id, buy_order.order_id, sell_order.order_id, symbol, exec_price, match_qty, now};
    result.fills.push_back(fill);

    // 2. Update remaining quantities
    buy_order.remaining_quantity -= match_qty;
    sell_order.remaining_quantity -= match_qty;

    // 3. Generate Execution Report for resting buy order
    ExecutionReport buy_report;
    buy_report.order_id = buy_order.order_id;
    buy_report.symbol = buy_order.symbol;
    buy_report.side = buy_order.side;
    buy_report.price = buy_order.price;
    buy_report.quantity = buy_order.quantity;
    buy_report.remaining_quantity = buy_order.remaining_quantity;
    buy_report.last_qty = match_qty;
    buy_report.last_px = exec_price;
    buy_report.timestamp = now;
    buy_report.status = (buy_order.remaining_quantity == 0) ? OrderStatus::Filled : OrderStatus::PartiallyFilled;
    buy_report.client_id = buy_order.client_id;
    result.execution_reports.push_back(buy_report);

    // 4. Generate Execution Report for sell order
    ExecutionReport sell_report;
    sell_report.order_id = sell_order.order_id;
    sell_report.symbol = sell_order.symbol;
    sell_report.side = sell_order.side;
    sell_report.price = sell_order.price;
    sell_report.quantity = sell_order.quantity;
    sell_report.remaining_quantity = sell_order.remaining_quantity;
    sell_report.last_qty = match_qty;
    sell_report.last_px = exec_price;
    sell_report.timestamp = now;
    sell_report.status = (sell_order.remaining_quantity == 0) ? OrderStatus::Filled : OrderStatus::PartiallyFilled;
    sell_report.client_id = sell_order.client_id;
    result.execution_reports.push_back(sell_report);
}

} // namespace exchange
