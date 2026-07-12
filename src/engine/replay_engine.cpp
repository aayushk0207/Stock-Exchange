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
        LOG_WARN("ReplayEngine: Could not open WAL file for reading: " + log_filepath);
        return false;
    }

    LOG_INFO("ReplayEngine: Starting state recovery from WAL log: " + log_filepath);

    // Reset TradeIDGenerator so regenerated match trade IDs match original progression
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
        else if (type == WALEventType::Fill) {
            // Fill events are stored for transaction auditing/downstream logging.
            // Replaying Submits/Cancels/Modifies natively reproduces the exact matches
            // in the OrderBook state. So we skip manual fill execution during replay.
        }
    }

    LOG_INFO("ReplayEngine: Recovery complete. Replayed " + std::to_string(replayed_count) + " state-changing events.");
    return true;
}

} // namespace exchange
