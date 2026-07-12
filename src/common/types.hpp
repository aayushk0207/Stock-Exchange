#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "enums.hpp"

namespace exchange {

// Type aliases for core domain concepts
using Price = uint64_t;       // Fixed-point representation (e.g., cents or 4 decimal places)
using Quantity = uint32_t;
using OrderID = uint64_t;
using Symbol = std::string;
using Timestamp = uint64_t;   // Nanoseconds since epoch

// Represents an order submitted to the exchange
struct Order {
    OrderID order_id{0};
    Symbol symbol;
    Side side{Side::Buy};
    OrderType type{OrderType::Limit};
    Price price{0};
    Quantity quantity{0};
    Quantity remaining_quantity{0};
    TimeInForce tif{TimeInForce::GTC};
    Timestamp timestamp{0};
    uint32_t client_id{0}; // Track originating client session

    bool is_filled() const {
        return remaining_quantity == 0;
    }
};

// Represents a request to cancel an active order
struct CancelRequest {
    OrderID order_id{0};
    Symbol symbol;
    Timestamp timestamp{0};
    uint32_t client_id{0}; // Originating client session
};

// Represents a request to modify an active order's price/quantity
struct ModifyRequest {
    OrderID order_id{0};
    Symbol symbol;
    Price price{0};
    Quantity quantity{0};
    Timestamp timestamp{0};
    uint32_t client_id{0}; // Originating client session
};

// Represents a trade execution/fill event
struct Fill {
    uint64_t trade_id{0};
    OrderID buy_order_id{0};
    OrderID sell_order_id{0};
    Symbol symbol;
    Price price{0};
    Quantity quantity{0};
    Timestamp timestamp{0};
};

// Represents a public trade event broadcasted to all market data clients
struct Trade {
    uint64_t trade_id{0};
    Symbol symbol;
    Price price{0};
    Quantity quantity{0};
    Timestamp timestamp{0};
};

// Represents an execution report sent to the client gateway
struct ExecutionReport {
    OrderID order_id{0};
    Symbol symbol;
    Side side{Side::Buy};
    OrderStatus status{OrderStatus::New};
    Price price{0};
    Quantity quantity{0};
    Quantity remaining_quantity{0};
    Quantity last_qty{0};
    Price last_px{0};
    Timestamp timestamp{0};
    std::string reject_reason;
    uint32_t client_id{0}; // Destination client session
};

// Represents the result of an order match attempt in the order book
struct MatchResult {
    OrderStatus status{OrderStatus::New};
    std::vector<Fill> fills;
    std::vector<ExecutionReport> execution_reports;
};

inline std::string to_string(Side side) {
    return side == Side::Buy ? "BUY" : "SELL";
}

inline std::string to_string(OrderStatus status) {
    switch (status) {
        case OrderStatus::New: return "NEW";
        case OrderStatus::PartiallyFilled: return "PARTIALLY_FILLED";
        case OrderStatus::Filled: return "FILLED";
        case OrderStatus::Cancelled: return "CANCELLED";
        case OrderStatus::Rejected: return "REJECTED";
    }
    return "UNKNOWN";
}

} // namespace exchange
