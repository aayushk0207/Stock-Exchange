#pragma once

#include "../common/types.hpp"
#include <memory>

namespace exchange {

class BookRegistry;
class ExecutionPipeline;
class WAL;
class OrderBook;

class MatchingEngine {
public:
    MatchingEngine(BookRegistry& registry, ExecutionPipeline& pipeline, WAL& wal);
    ~MatchingEngine() = default;

    // Prevent copying
    MatchingEngine(const MatchingEngine&) = delete;
    MatchingEngine& operator=(const MatchingEngine&) = delete;

    // Coordinator entry points for gateway/inbound messages
    void process_order(const Order& order);
    void process_cancel(OrderID order_id, const Symbol& symbol);
    void process_modify(const ModifyRequest& request);

    // Pure matching algorithm between two crossing orders
    static void matchOrders(Order& buy_order, Order& sell_order, const Symbol& symbol, MatchResult& result);

private:
    BookRegistry& registry_;
    ExecutionPipeline& pipeline_;
    WAL& wal_;
};

} // namespace exchange
