#include "matching_engine.hpp"
#include "order_book.hpp"
#include "../registry/book_registry.hpp"
#include "../pipeline/wal.hpp"
#include "../pipeline/execution_pipeline.hpp"
#include "../logger/logger.hpp"
#include "../common/time_utils.hpp"
#include "../common/trade_id_generator.hpp"

namespace exchange {

void MatchingEngine::matchOrders(Order& buy_order, Order& sell_order, const Symbol& symbol, MatchResult& result) {
    Quantity match_qty = std::min(buy_order.remaining_quantity, sell_order.remaining_quantity);
    if (match_qty == 0) {
        return;
    }

    uint64_t trade_id = TradeIDGenerator::get_instance().next_id();
    Timestamp now = time_utils::get_current_time_ns();
    Price exec_price = (sell_order.timestamp < buy_order.timestamp) ? sell_order.price : buy_order.price;

    LOG_INFO("[MATCH] Buy order " + std::to_string(buy_order.order_id) +
             " matched with Sell order " + std::to_string(sell_order.order_id) +
             " (price=" + std::to_string(exec_price) + ", qty=" + std::to_string(match_qty) + ")");

    Fill fill{trade_id, buy_order.order_id, sell_order.order_id, symbol, exec_price, match_qty, now};
    result.fills.push_back(fill);

    buy_order.remaining_quantity -= match_qty;
    sell_order.remaining_quantity -= match_qty;

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

}
