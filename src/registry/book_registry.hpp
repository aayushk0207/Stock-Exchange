#pragma once

#include "../common/types.hpp"
#include <unordered_map>
#include <memory>
#include <mutex>
#include <optional>
#include "../engine/order_book.hpp"

namespace exchange {


class BookRegistry {
public:
    BookRegistry() = default;
    ~BookRegistry();

    // Prevent copying
    BookRegistry(const BookRegistry&) = delete;
    BookRegistry& operator=(const BookRegistry&) = delete;

    // Register a new symbol and create its order book
    bool register_symbol(const Symbol& symbol);

    // Get order book pointer for symbol
    OrderBook* get_order_book(const Symbol& symbol);

    // Check if symbol exists
    bool has_symbol(const Symbol& symbol) const;

private:
    std::unordered_map<Symbol, std::unique_ptr<OrderBook>> books_;
    mutable std::mutex mutex_;
};

} // namespace exchange
