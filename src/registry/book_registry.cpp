#include "book_registry.hpp"
#include "../engine/order_book.hpp"
#include "../logger/logger.hpp"

namespace exchange {

BookRegistry::~BookRegistry() = default;

/*
bool BookRegistry::register_symbol(const Symbol& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (books_.find(symbol) != books_.end()) {
        LOG_WARN("Symbol " + symbol + " is already registered.");
        return false;
    }

    books_[symbol] = std::make_unique<OrderBook>(symbol);
    LOG_INFO("Registered new symbol: " + symbol);
    return true;
}
*/

OrderBook* BookRegistry::get_order_book(const Symbol& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = books_.find(symbol);
    if (it != books_.end()) {
        return it->second.get();
    }
    books_[symbol] = std::make_unique<OrderBook>(symbol);
    LOG_INFO("Auto-created order book for symbol: " + symbol);
    return books_[symbol].get();
}

/*
bool BookRegistry::has_symbol(const Symbol& symbol) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return books_.find(symbol) != books_.end();
}
*/

} // namespace exchange
