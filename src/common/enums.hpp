#pragma once

#include <cstdint>

namespace exchange {

enum class Side : uint8_t {
    Buy,
    Sell
};

enum class OrderType : uint8_t {
    Limit,
    Market
};

enum class TimeInForce : uint8_t {
    GTC,  // Good 'Till Cancelled
    IOC,  // Immediate Or Cancel
    FOK   // Fill Or Kill
};

enum class OrderStatus : uint8_t {
    New,
    PartiallyFilled,
    Filled,
    Cancelled,
    Rejected
};

} // namespace exchange
