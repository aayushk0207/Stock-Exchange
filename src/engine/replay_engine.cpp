#include "replay_engine.hpp"
#include "../registry/book_registry.hpp"
#include "../engine/order_book.hpp"
#include "../pipeline/wal_event.hpp"
#include "../common/trade_id_generator.hpp"
#include "../logger/logger.hpp"
#include <fstream>

namespace exchange {

bool ReplayEngine::replay(const std::string& log_filepath, BookRegistry& registry) {
    std::ifstream file(log_filepath, std::ios::binary | std::ios::in);
    if (!file.is_open()) {
        LOG_WARN("ReplayEngine: Could not open WAL file: " + log_filepath);
        return false;
    }

    LOG_INFO("ReplayEngine: Replaying WAL log file: " + log_filepath);
    TradeIDGenerator::get_instance().reset(1);

    WALEntry entry;
    size_t replayed_count = 0;

    while (file.read(reinterpret_cast<char*>(&entry), sizeof(WALEntry))) {
        WALEventType type = static_cast<WALEventType>(entry.type);

        if (type == WALEventType::Submit) {
            Order order = BinarySerializer::from_net(entry.data.order);
            OrderBook* book = registry.get_order_book(order.symbol);
            book->submitOrder(order);
            replayed_count++;
        } 
        else if (type == WALEventType::Cancel) {
            CancelRequest req = BinarySerializer::from_net(entry.data.cancel);
            OrderBook* book = registry.get_order_book(req.symbol);
            MatchResult res;
            book->cancelOrder(req.order_id, res);
            replayed_count++;
        } 
        else if (type == WALEventType::Modify) {
            ModifyRequest req = BinarySerializer::from_net(entry.data.modify);
            OrderBook* book = registry.get_order_book(req.symbol);
            MatchResult res;
            book->modifyOrder(req, res);
            replayed_count++;
        }
    }

    LOG_INFO("ReplayEngine: Recovery complete (" + std::to_string(replayed_count) + " events).");
    return true;
}

}
