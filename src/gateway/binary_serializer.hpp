#pragma once

#include "../common/types.hpp"
#include <vector>
#include <cstring>
#include <string>

namespace exchange {

// Message Types
enum class MsgType : uint16_t {
    Order = 1,
    Cancel = 2,
    Modify = 3,
    ExecutionReport = 4
};

#pragma pack(push, 1)
struct MsgHeader {
    uint16_t type;
    uint16_t length;
};

struct NetOrder {
    uint64_t order_id;
    char symbol[8];
    uint8_t side;
    uint8_t type;
    uint64_t price;
    uint32_t quantity;
    uint64_t timestamp;
};

struct NetCancel {
    uint64_t order_id;
    char symbol[8];
    uint64_t timestamp;
};

struct NetModify {
    uint64_t order_id;
    char symbol[8];
    uint64_t price;
    uint32_t quantity;
    uint64_t timestamp;
};

struct NetExecutionReport {
    uint64_t order_id;
    char symbol[8];
    uint8_t side;
    uint8_t status;
    uint64_t price;
    uint32_t quantity;
    uint32_t remaining_quantity;
    uint32_t last_qty;
    uint64_t last_px;
    uint64_t timestamp;
    char reject_reason[32];
};
#pragma pack(pop)

class BinarySerializer {
public:
    // Helper to safely copy standard strings into fixed character buffers
    static void safe_strncpy(char* dest, const std::string& src, size_t max_len) {
        size_t len = std::min(src.size(), max_len - 1);
        std::memcpy(dest, src.c_str(), len);
        dest[len] = '\0';
    }

    // Convert from internal Order to NetOrder
    static NetOrder to_net(const Order& order) {
        NetOrder net;
        net.order_id = order.order_id;
        safe_strncpy(net.symbol, order.symbol, sizeof(net.symbol));
        net.side = static_cast<uint8_t>(order.side);
        net.type = static_cast<uint8_t>(order.type);
        net.price = order.price;
        net.quantity = order.quantity;
        net.timestamp = order.timestamp;
        return net;
    }

    // Convert from NetOrder to internal Order
    static Order from_net(const NetOrder& net) {
        Order order;
        order.order_id = net.order_id;
        order.symbol = std::string(net.symbol);
        order.side = static_cast<Side>(net.side);
        order.type = static_cast<OrderType>(net.type);
        order.price = net.price;
        order.quantity = net.quantity;
        order.remaining_quantity = net.quantity;
        order.timestamp = net.timestamp;
        return order;
    }

    // Convert from NetCancel to internal CancelRequest
    static CancelRequest from_net(const NetCancel& net) {
        CancelRequest req;
        req.order_id = net.order_id;
        req.symbol = std::string(net.symbol);
        req.timestamp = net.timestamp;
        return req;
    }

    // Convert from internal CancelRequest to NetCancel
    static NetCancel to_net(const CancelRequest& req) {
        NetCancel net;
        net.order_id = req.order_id;
        safe_strncpy(net.symbol, req.symbol, sizeof(net.symbol));
        net.timestamp = req.timestamp;
        return net;
    }

    // Convert from NetModify to internal ModifyRequest
    static ModifyRequest from_net(const NetModify& net) {
        ModifyRequest req;
        req.order_id = net.order_id;
        req.symbol = std::string(net.symbol);
        req.price = net.price;
        req.quantity = net.quantity;
        req.timestamp = net.timestamp;
        return req;
    }

    // Convert from internal ModifyRequest to NetModify
    static NetModify to_net(const ModifyRequest& req) {
        NetModify net;
        net.order_id = req.order_id;
        safe_strncpy(net.symbol, req.symbol, sizeof(net.symbol));
        net.price = req.price;
        net.quantity = req.quantity;
        net.timestamp = req.timestamp;
        return net;
    }

    // Convert from internal ExecutionReport to NetExecutionReport
    static NetExecutionReport to_net(const ExecutionReport& report) {
        NetExecutionReport net;
        net.order_id = report.order_id;
        safe_strncpy(net.symbol, report.symbol, sizeof(net.symbol));
        net.side = static_cast<uint8_t>(report.side);
        net.status = static_cast<uint8_t>(report.status);
        net.price = report.price;
        net.quantity = report.quantity;
        net.remaining_quantity = report.remaining_quantity;
        net.last_qty = report.last_qty;
        net.last_px = report.last_px;
        net.timestamp = report.timestamp;
        safe_strncpy(net.reject_reason, report.reject_reason, sizeof(net.reject_reason));
        return net;
    }
};

}
