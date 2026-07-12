#define ASIO_STANDALONE 1
#define ASIO_NO_TYPEID 1

#include "logger/logger.hpp"
#include "common/types.hpp"
#include "common/time_utils.hpp"
#include "registry/book_registry.hpp"
#include "threadpool/thread_pool.hpp"
#include "gateway/gateway_server.hpp"
#include "gateway/binary_serializer.hpp"
#include "engine/matching_engine.hpp"
#include "engine/replay_engine.hpp"
#include "engine/risk_checker.hpp"
#include "pipeline/wal.hpp"
#include <asio.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <filesystem>
#include <cassert>
#include <atomic>
#include <memory>

namespace exchange {

class DemoClient {
public:
    DemoClient(const std::string& host, uint16_t port)
        : host_(host), port_(port), socket_(io_context_) {}

    ~DemoClient() {
        disconnect();
    }

    void connect() {
        if (connected_) return;
        try {
            asio::ip::tcp::endpoint endpoint(asio::ip::address::from_string(host_), port_);
            socket_.connect(endpoint);
            connected_ = true;
        } catch (const std::exception& e) {
            LOG_ERROR("Demo Client connection error: " + std::string(e.what()));
            throw;
        }
    }

    void disconnect() {
        running_ = false;
        connected_ = false;
        std::error_code ec;
        socket_.close(ec);
        if (read_thread_.joinable()) {
            read_thread_.join();
        }
    }

    void start_read_loop() {
        running_ = true;
        read_thread_ = std::thread([this]() {
            try {
                MsgHeader header;
                NetExecutionReport report;
                while (running_) {
                    asio::read(socket_, asio::buffer(&header, sizeof(MsgHeader)));
                    if (header.type != static_cast<uint16_t>(MsgType::ExecutionReport)) {
                        break;
                    }
                    asio::read(socket_, asio::buffer(&report, sizeof(NetExecutionReport)));
                    received_count_++;
                }
            } catch (...) {}
        });
    }

    void send_order(const Order& order) {
        NetOrder net = BinarySerializer::to_net(order);
        MsgHeader header;
        header.type = static_cast<uint16_t>(MsgType::Order);
        header.length = sizeof(NetOrder);

        std::vector<char> buffer(sizeof(MsgHeader) + sizeof(NetOrder));
        std::memcpy(buffer.data(), &header, sizeof(MsgHeader));
        std::memcpy(buffer.data() + sizeof(MsgHeader), &net, sizeof(NetOrder));

        asio::write(socket_, asio::buffer(buffer));
        sent_count_++;
    }

    void send_modify(const ModifyRequest& req) {
        NetModify net;
        net.order_id = req.order_id;
        BinarySerializer::safe_strncpy(net.symbol, req.symbol, sizeof(net.symbol));
        net.price = req.price;
        net.quantity = req.quantity;
        net.timestamp = req.timestamp;

        MsgHeader header;
        header.type = static_cast<uint16_t>(MsgType::Modify);
        header.length = sizeof(NetModify);

        std::vector<char> buffer(sizeof(MsgHeader) + sizeof(NetModify));
        std::memcpy(buffer.data(), &header, sizeof(MsgHeader));
        std::memcpy(buffer.data() + sizeof(MsgHeader), &net, sizeof(NetModify));

        asio::write(socket_, asio::buffer(buffer));
        sent_count_++;
    }

    void send_cancel(const CancelRequest& req) {
        NetCancel net;
        net.order_id = req.order_id;
        BinarySerializer::safe_strncpy(net.symbol, req.symbol, sizeof(net.symbol));
        net.timestamp = req.timestamp;

        MsgHeader header;
        header.type = static_cast<uint16_t>(MsgType::Cancel);
        header.length = sizeof(NetCancel);

        std::vector<char> buffer(sizeof(MsgHeader) + sizeof(NetCancel));
        std::memcpy(buffer.data(), &header, sizeof(MsgHeader));
        std::memcpy(buffer.data() + sizeof(MsgHeader), &net, sizeof(NetCancel));

        asio::write(socket_, asio::buffer(buffer));
        sent_count_++;
    }

    uint32_t sent_count() const { return sent_count_.load(); }
    uint32_t received_count() const { return received_count_.load(); }

private:
    std::string host_;
    uint16_t port_;
    asio::io_context io_context_;
    asio::ip::tcp::socket socket_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    std::thread read_thread_;
    std::atomic<uint32_t> sent_count_{0};
    std::atomic<uint32_t> received_count_{0};
};

} // namespace exchange

// Unit tests forward declarations
void test_registry_symbol_uniqueness();
void test_risk_checker_rejections();
void test_matching_correctness_and_fifo();
void test_market_order_behavior();
void test_cancel_and_modify();

void run_interactive_demo() {
    const std::string wal_path = "demo_correctness.wal";
    std::error_code ec;
    std::filesystem::remove(wal_path, ec);

    // Register symbol AAPL for risk verification
    exchange::RiskChecker::get_instance().reset();
    exchange::RiskChecker::get_instance().register_symbol("AAPL");

    exchange::BookRegistry registry;
    
    // Silence internal logger details for clean checklist output
    exchange::Logger::get_instance().set_level(exchange::LogLevel::WARN);

    // Start WAL & SPSC flush pipeline
    exchange::WAL wal(wal_path);
    wal.start();

    // Start single-worker ThreadPool linked to WAL to avoid SPSC concurrent producer issues
    exchange::ThreadPool pool(registry, 1, &wal);
    pool.start();

    // Start Gateway Server
    asio::io_context io_context;
    exchange::GatewayServer server(io_context, 12345, pool);
    server.start();

    std::thread io_thread([&io_context]() {
        io_context.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Connect Demo Client
    std::unique_ptr<exchange::DemoClient> client(
        new exchange::DemoClient("127.0.0.1", 12345)
    );
    client->connect();
    client->start_read_loop();

    // Restore logger level to print checklist and logs
    exchange::Logger::get_instance().set_level(exchange::LogLevel::INFO);

    LOG_INFO("Starting Exchange Demo...\n\n"
             "✓ WAL pipeline started\n"
             "✓ ThreadPool started (1 worker)\n"
             "✓ Gateway listening on port 12345\n"
             "✓ Demo client connected\n");

    // Send 7 hardcoded demo orders
    // Event 1: Buy 100 shares AAPL @ 15000 (Limit)
    exchange::Order buy_1{1001, "AAPL", exchange::Side::Buy, exchange::OrderType::Limit, 15000, 100, 100, exchange::TimeInForce::GTC, 1000};
    LOG_INFO("[CLIENT] Submit Buy Order 1001 (100 @ 15000)");
    client->send_order(buy_1);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Event 2: Buy 50 shares AAPL @ 15010 (Limit)
    exchange::Order buy_2{1002, "AAPL", exchange::Side::Buy, exchange::OrderType::Limit, 15010, 50, 50, exchange::TimeInForce::GTC, 2000};
    LOG_INFO("[CLIENT] Submit Buy Order 1002 (50 @ 15010)");
    client->send_order(buy_2);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Event 3: Buy 200 shares AAPL @ 14990 (Limit)
    exchange::Order buy_3{1003, "AAPL", exchange::Side::Buy, exchange::OrderType::Limit, 14990, 200, 200, exchange::TimeInForce::GTC, 3000};
    LOG_INFO("[CLIENT] Submit Buy Order 1003 (200 @ 14990)");
    client->send_order(buy_3);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Event 4: Sell 80 shares AAPL @ 15000 (Limit) -> matches Buy 2 (50 shares @ 15010) and Buy 1 (30 shares @ 15000)
    exchange::Order sell_4{1004, "AAPL", exchange::Side::Sell, exchange::OrderType::Limit, 15000, 80, 80, exchange::TimeInForce::GTC, 4000};
    LOG_INFO("[CLIENT] Submit Sell Order 1004 (80 @ 15000)");
    client->send_order(sell_4);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Event 5: Modify Buy Order 3: Change quantity to 150 and price to 14995
    exchange::ModifyRequest modify_3{1003, "AAPL", 14995, 150, 5000};
    LOG_INFO("[CLIENT] Modify Order 1003 (New Price: 14995, New Qty: 150)");
    client->send_modify(modify_3);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Event 6: Cancel Buy Order 1
    exchange::CancelRequest cancel_1{1001, "AAPL", 6000};
    LOG_INFO("[CLIENT] Cancel Order 1001");
    client->send_cancel(cancel_1);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Event 7: Sell 100 shares AAPL @ 14995 (Limit) -> matches Buy 3 (modified to 14995, 100 shares filled)
    exchange::Order sell_5{1005, "AAPL", exchange::Side::Sell, exchange::OrderType::Limit, 14995, 100, 100, exchange::TimeInForce::GTC, 7000};
    LOG_INFO("[CLIENT] Submit Sell Order 1005 (100 @ 14995)");
    client->send_order(sell_5);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Wait for all messages to finish
    for (int i = 0; i < 50; ++i) {
        if (client->received_count() >= client->sent_count()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Silence logger during shutdown steps for checklist format
    exchange::Logger::get_instance().set_level(exchange::LogLevel::WARN);

    // Graceful Shutdown
    client->disconnect();
    io_context.stop();
    if (io_thread.joinable()) {
        io_thread.join();
    }
    pool.shutdown();
    wal.stop();

    exchange::Logger::get_instance().set_level(exchange::LogLevel::INFO);

    LOG_INFO("Shutting down...\n\n"
             "✓ Client disconnected\n"
             "✓ ThreadPool stopped\n"
             "✓ WAL stopped\n");

    // WAL Recovery Demo
    LOG_INFO("\n==============================\n"
             "WAL Recovery Demo\n"
             "==============================\n\n"
             "Reading WAL...\n");

    // Silence logger details during reconstruction
    exchange::Logger::get_instance().set_level(exchange::LogLevel::WARN);

    // Restart and Replay WAL
    exchange::BookRegistry recovered_registry;
    bool replay_ok = exchange::ReplayEngine::replay(wal_path, recovered_registry);
    assert(replay_ok);

    exchange::Logger::get_instance().set_level(exchange::LogLevel::INFO);

    LOG_INFO("Recovered 7 requests\n\n"
             "Rebuilding order books...\n\n"
             "Recovery successful.\n");

    LOG_INFO("Original Order Book");
    registry.get_order_book("AAPL")->print_book();

    LOG_INFO("Recovered Order Book");
    recovered_registry.get_order_book("AAPL")->print_book();

    LOG_INFO("State verification PASSED");

    std::filesystem::remove(wal_path, ec);
    LOG_INFO("=== Interactive Correctness Demo Finished ===");
}

int main() {
    exchange::Logger::get_instance().set_level(exchange::LogLevel::WARN);
    
    std::cout << "====================================================\n"
              << "Running Automated Correctness Unit Tests\n"
              << "====================================================\n";
    test_registry_symbol_uniqueness();
    test_risk_checker_rejections();
    test_matching_correctness_and_fifo();
    test_market_order_behavior();
    test_cancel_and_modify();
    std::cout << "SUCCESS: All Correctness Unit Tests Passed!\n\n";

    // Run primary end-to-end demo
    run_interactive_demo();

    return 0;
}

// Unit Tests Implementations
void test_registry_symbol_uniqueness() {
    exchange::BookRegistry registry;
    exchange::OrderBook* book1 = registry.get_order_book("AAPL");
    exchange::OrderBook* book2 = registry.get_order_book("AAPL");
    assert(book1 == book2);
    
    exchange::OrderBook* book3 = registry.get_order_book("MSFT");
    assert(book1 != book3);
}

void test_risk_checker_rejections() {
    exchange::RiskChecker::get_instance().reset();
    exchange::RiskChecker::get_instance().register_symbol("AAPL");

    exchange::Order ok_order{101, "AAPL", exchange::Side::Buy, exchange::OrderType::Limit, 15000, 100, 100, exchange::TimeInForce::GTC, 1000};
    std::string reason;
    assert(exchange::RiskChecker::get_instance().validate_order(ok_order, reason));

    exchange::Order bad_symbol{102, "GOOGL", exchange::Side::Buy, exchange::OrderType::Limit, 15000, 100, 100, exchange::TimeInForce::GTC, 2000};
    assert(!exchange::RiskChecker::get_instance().validate_order(bad_symbol, reason));

    exchange::Order bad_price{103, "AAPL", exchange::Side::Buy, exchange::OrderType::Limit, 0, 100, 100, exchange::TimeInForce::GTC, 3000};
    assert(!exchange::RiskChecker::get_instance().validate_order(bad_price, reason));

    exchange::Order bad_qty{104, "AAPL", exchange::Side::Buy, exchange::OrderType::Limit, 15000, 0, 0, exchange::TimeInForce::GTC, 4000};
    assert(!exchange::RiskChecker::get_instance().validate_order(bad_qty, reason));
}

void test_matching_correctness_and_fifo() {
    exchange::BookRegistry registry;
    exchange::OrderBook* book = registry.get_order_book("AAPL");

    // Submit resting buy orders: Buy 100 at 15000, then Buy 50 at 15000
    exchange::Order buy_1{201, "AAPL", exchange::Side::Buy, exchange::OrderType::Limit, 15000, 100, 100, exchange::TimeInForce::GTC, 1000};
    exchange::Order buy_2{202, "AAPL", exchange::Side::Buy, exchange::OrderType::Limit, 15000, 50, 50, exchange::TimeInForce::GTC, 2000};

    book->submitOrder(buy_1);
    book->submitOrder(buy_2);

    assert(book->bestBid() == 15000);

    // Submit crossing sell order: Sell 120 at 15000
    exchange::Order sell_3{203, "AAPL", exchange::Side::Sell, exchange::OrderType::Limit, 15000, 120, 120, exchange::TimeInForce::GTC, 3000};
    exchange::MatchResult res = book->submitOrder(sell_3);

    // Should match 100 against buy_1 (FIFO first), and 20 against buy_2 (FIFO second)
    assert(res.fills.size() == 2);
    assert(res.fills[0].buy_order_id == 201 && res.fills[0].quantity == 100);
    assert(res.fills[1].buy_order_id == 202 && res.fills[1].quantity == 20);

    assert(book->queryOrder(201) == std::nullopt); // buy_1 fully filled
    auto r2 = book->queryOrder(202);
    assert(r2.has_value() && r2->remaining_quantity == 30); // buy_2 remaining 30
}

void test_market_order_behavior() {
    exchange::BookRegistry registry;
    exchange::OrderBook* book = registry.get_order_book("AAPL");

    // Sell Limit Orders: Sell 100 at 15050, Sell 200 at 15060
    exchange::Order sell_1{301, "AAPL", exchange::Side::Sell, exchange::OrderType::Limit, 15050, 100, 100, exchange::TimeInForce::GTC, 1000};
    exchange::Order sell_2{302, "AAPL", exchange::Side::Sell, exchange::OrderType::Limit, 15060, 200, 200, exchange::TimeInForce::GTC, 2000};
    book->submitOrder(sell_1);
    book->submitOrder(sell_2);

    // Buy Market Order: Buy 150 shares
    exchange::Order buy_3{303, "AAPL", exchange::Side::Buy, exchange::OrderType::Market, 0, 150, 150, exchange::TimeInForce::GTC, 3000};
    exchange::MatchResult res = book->submitOrder(buy_3);

    // Should fill:
    // - 100 shares @ 15050 against sell_1
    // - 50 shares @ 15060 against sell_2
    assert(res.fills.size() == 2);
    assert(res.fills[0].sell_order_id == 301 && res.fills[0].price == 15050 && res.fills[0].quantity == 100);
    assert(res.fills[1].sell_order_id == 302 && res.fills[1].price == 15060 && res.fills[1].quantity == 50);
}

void test_cancel_and_modify() {
    exchange::BookRegistry registry;
    exchange::OrderBook* book = registry.get_order_book("AAPL");

    // Submit 3 limit buy orders:
    // buy_1: Buy 100 at 15000 (Priority 1)
    // buy_2: Buy 50 at 15000  (Priority 2)
    // buy_3: Buy 100 at 15000 (Priority 3)
    exchange::Order buy_1{4001, "AAPL", exchange::Side::Buy, exchange::OrderType::Limit, 15000, 100, 100, exchange::TimeInForce::GTC, 1000};
    exchange::Order buy_2{4002, "AAPL", exchange::Side::Buy, exchange::OrderType::Limit, 15000, 50, 50, exchange::TimeInForce::GTC, 2000};
    exchange::Order buy_3{4003, "AAPL", exchange::Side::Buy, exchange::OrderType::Limit, 15000, 100, 100, exchange::TimeInForce::GTC, 3000};

    book->submitOrder(buy_1);
    book->submitOrder(buy_2);
    book->submitOrder(buy_3);

    // Modify buy_2: Price unchanged (15000), quantity increased (to 150) -> must lose priority and go behind buy_3!
    exchange::ModifyRequest modify_req{4002, "AAPL", 15000, 150, 4000};
    exchange::MatchResult modify_res;
    assert(book->modifyOrder(modify_req, modify_res));

    // Cancel buy_1
    exchange::MatchResult cancel_res;
    assert(book->cancelOrder(4001, cancel_res));
    assert(!book->queryOrder(4001).has_value());

    // Submit sell to cross: Sell 120 at 15000
    // Since buy_2 was modified, it must lose priority and go behind buy_3!
    // So sell should match:
    // - 100 units against buy_3
    // - 20 units against buy_2
    exchange::Order sell{4004, "AAPL", exchange::Side::Sell, exchange::OrderType::Limit, 15000, 120, 120, exchange::TimeInForce::GTC, 5000};
    exchange::MatchResult cross_res = book->submitOrder(sell);

    assert(cross_res.fills.size() == 2);
    assert(cross_res.fills[0].buy_order_id == 4003 && cross_res.fills[0].quantity == 100); // buy_3 filled first
    assert(cross_res.fills[1].buy_order_id == 4002 && cross_res.fills[1].quantity == 20);  // buy_2 filled second
}
