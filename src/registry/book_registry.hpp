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

    BookRegistry(const BookRegistry&) = delete;
    BookRegistry& operator=(const BookRegistry&) = delete;

    OrderBook* get_order_book(const Symbol& symbol);

private:
    std::unordered_map<Symbol, std::unique_ptr<OrderBook>> books_;
    mutable std::mutex mutex_;
};

}
