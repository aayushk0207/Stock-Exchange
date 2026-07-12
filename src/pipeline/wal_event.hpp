#pragma once

#include <cstdint>
#include "../gateway/binary_serializer.hpp"

namespace exchange {

enum class WALEventType : uint8_t {
    Submit = 1,
    Cancel = 2,
    Modify = 3,
    Fill = 4
};

#pragma pack(push, 1)
struct NetFill {
    uint64_t trade_id;
    uint64_t buy_order_id;
    uint64_t sell_order_id;
    char symbol[8];
    uint64_t price;
    uint32_t quantity;
    uint64_t timestamp;
};

struct WALEntry {
    uint8_t type; // WALEventType
    union {
        NetOrder order;
        NetCancel cancel;
        NetModify modify;
        NetFill fill;
    } data;
};
#pragma pack(pop)

} // namespace exchange
