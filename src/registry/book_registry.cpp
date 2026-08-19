#include "book_registry.hpp"
#include "../engine/order_book.hpp"
#include "../logger/logger.hpp"

namespace exchange {

BookRegistry::~BookRegistry() = default;

OrderBook* BookRegistry::get_order_book(const Symbol& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = books_.find(symbol);
    if (it != books_.end()) {
        return it->second.get();
    }
    books_[symbol] = std::make_unique<OrderBook>(symbol);
    LOG_INFO("Created order book for symbol: " + symbol);
    return books_[symbol].get();
}

}
