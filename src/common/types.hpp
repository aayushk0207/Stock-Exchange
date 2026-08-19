#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "enums.hpp"

namespace exchange {

using Price = uint64_t;
using Quantity = uint32_t;
using OrderID = uint64_t;
using Symbol = std::string;
using Timestamp = uint64_t;

struct Order {
    OrderID order_id{0};
    Symbol symbol;
    Side side{Side::Buy};
    OrderType type{OrderType::Limit};
    Price price{0};
    Quantity quantity{0};
    Quantity remaining_quantity{0};
    Timestamp timestamp{0};
    uint32_t client_id{0};
    bool is_filled() const {
        return remaining_quantity == 0;
    }
};

struct CancelRequest {
    OrderID order_id{0};
    Symbol symbol;
    Timestamp timestamp{0};
    uint32_t client_id{0};
};

struct ModifyRequest {
    OrderID order_id{0};
    Symbol symbol;
    Price price{0};
    Quantity quantity{0};
    Timestamp timestamp{0};
    uint32_t client_id{0};
};

struct Fill {
    uint64_t trade_id{0};
    OrderID buy_order_id{0};
    OrderID sell_order_id{0};
    Symbol symbol;
    Price price{0};
    Quantity quantity{0};
    Timestamp timestamp{0};
};

struct Trade {
    uint64_t trade_id{0};
    Symbol symbol;
    Price price{0};
    Quantity quantity{0};
    Timestamp timestamp{0};
};

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
    uint32_t client_id{0};
};

struct MatchResult {
    OrderStatus status{OrderStatus::New};
    std::vector<Fill> fills;
    std::vector<ExecutionReport> execution_reports;
};

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

}
