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
    MatchingEngine() = delete;
    static void matchOrders(Order& buy_order, Order& sell_order, const Symbol& symbol, MatchResult& result);
};

}
