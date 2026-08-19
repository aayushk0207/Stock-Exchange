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
#include "pipeline/execution_pipeline.hpp"
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
    DemoClient(const std::string& name, const std::string& host, uint16_t port)
        : name_(name), host_(host), port_(port), socket_(io_context_) {}

    ~DemoClient() {
        disconnect();
    }

    const std::string& name() const { return name_; }

    void connect() {
        if (connected_) return;
        try {
            asio::ip::tcp::endpoint endpoint(asio::ip::address::from_string(host_), port_);
            socket_.connect(endpoint);
            connected_ = true;
        } catch (const std::exception& e) {
            LOG_ERROR("[" + name_ + "] Connection error: " + std::string(e.what()));
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
        NetModify net = BinarySerializer::to_net(req);

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
        NetCancel net = BinarySerializer::to_net(req);

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
    std::string name_;
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

}

void run_multi_client_demo() {
    const std::string wal_path = "multi_client_demo.wal";
    std::error_code ec;
    std::filesystem::remove(wal_path, ec);

    exchange::RiskChecker::get_instance().reset();
    exchange::RiskChecker::get_instance().register_symbol("AAPL");
    exchange::RiskChecker::get_instance().register_symbol("MSFT");

    exchange::BookRegistry registry;
    exchange::Logger::get_instance().set_level(exchange::LogLevel::WARN);

    exchange::WAL wal(wal_path);
    wal.start();

    exchange::ExecutionPipeline pipeline;
    pipeline.start();

    exchange::ThreadPool pool(registry, 4, &wal, &pipeline);
    pool.start();

    asio::io_context io_context;
    exchange::GatewayServer server(io_context, 12345, pool);
    server.start();

    std::thread io_thread([&io_context]() {
        io_context.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto client1 = std::make_unique<exchange::DemoClient>("Client-1", "127.0.0.1", 12345);
    auto client2 = std::make_unique<exchange::DemoClient>("Client-2", "127.0.0.1", 12345);
    auto client3 = std::make_unique<exchange::DemoClient>("Client-3", "127.0.0.1", 12345);

    client1->connect(); client1->start_read_loop();
    client2->connect(); client2->start_read_loop();
    client3->connect(); client3->start_read_loop();

    exchange::Logger::get_instance().set_level(exchange::LogLevel::INFO);
    LOG_INFO("Exchange server started on port 12345 with 3 clients connected.");

    LOG_INFO("[Client-1] Submitting order 999 (invalid symbol GOOGL)");
    exchange::Order invalid_order{999, "GOOGL", exchange::Side::Buy, exchange::OrderType::Limit, 15000, 100, 100, 1000};
    client1->send_order(invalid_order);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    LOG_INFO("[Client-1] Submitting buy order 1001 (100 AAPL @ 15000)");
    exchange::Order buy_1001{1001, "AAPL", exchange::Side::Buy, exchange::OrderType::Limit, 15000, 100, 100, 1001};
    client1->send_order(buy_1001);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    LOG_INFO("[Client-2] Submitting buy order 1002 (50 AAPL @ 15010)");
    exchange::Order buy_1002{1002, "AAPL", exchange::Side::Buy, exchange::OrderType::Limit, 15010, 50, 50, 1002};
    client2->send_order(buy_1002);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    LOG_INFO("[Client-3] Submitting buy order 1003 (200 AAPL @ 14990)");
    exchange::Order buy_1003{1003, "AAPL", exchange::Side::Buy, exchange::OrderType::Limit, 14990, 200, 200, 1003};
    client3->send_order(buy_1003);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    LOG_INFO("[Client-2] Submitting sell order 2001 (80 AAPL @ 15000)");
    exchange::Order sell_2001{2001, "AAPL", exchange::Side::Sell, exchange::OrderType::Limit, 15000, 80, 80, 2001};
    client2->send_order(sell_2001);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    LOG_INFO("[Client-3] Submitting sell order 3001 (100 MSFT @ 30000)");
    exchange::Order sell_3001{3001, "MSFT", exchange::Side::Sell, exchange::OrderType::Limit, 30000, 100, 100, 3001};
    client3->send_order(sell_3001);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    LOG_INFO("[Client-1] Submitting buy order 1004 (100 MSFT @ 30000)");
    exchange::Order buy_1004{1004, "MSFT", exchange::Side::Buy, exchange::OrderType::Limit, 30000, 100, 100, 1004};
    client1->send_order(buy_1004);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    LOG_INFO("[Client-3] Modifying order 1003 on AAPL (price=14995, qty=150)");
    exchange::ModifyRequest modify_1003{1003, "AAPL", 14995, 150, 5000};
    client3->send_modify(modify_1003);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    LOG_INFO("[Client-1] Cancelling order 1001 on AAPL");
    exchange::CancelRequest cancel_1001{1001, "AAPL", 6000};
    client1->send_cancel(cancel_1001);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    LOG_INFO("[Client-2] Submitting sell order 2002 (50 AAPL @ 15050)");
    exchange::Order sell_2002{2002, "AAPL", exchange::Side::Sell, exchange::OrderType::Limit, 15050, 50, 50, 7001};
    client2->send_order(sell_2002);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    LOG_INFO("[Client-1] Submitting market buy order 1005 (50 AAPL)");
    exchange::Order market_buy{1005, "AAPL", exchange::Side::Buy, exchange::OrderType::Market, 0, 50, 50, 7002};
    client1->send_order(market_buy);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    for (int i = 0; i < 50; ++i) {
        if (client1->received_count() >= client1->sent_count() &&
            client2->received_count() >= client2->sent_count() &&
            client3->received_count() >= client3->sent_count()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    exchange::Logger::get_instance().set_level(exchange::LogLevel::WARN);

    client1->disconnect();
    client2->disconnect();
    client3->disconnect();
    io_context.stop();
    if (io_thread.joinable()) {
        io_thread.join();
    }
    pool.shutdown();
    wal.stop();
    pipeline.stop();

    exchange::Logger::get_instance().set_level(exchange::LogLevel::INFO);
    LOG_INFO("Server shutdown complete.");

    LOG_INFO("Replaying WAL log file...");
    exchange::Logger::get_instance().set_level(exchange::LogLevel::WARN);

    exchange::BookRegistry recovered_registry;
    bool replay_ok = exchange::ReplayEngine::replay(wal_path, recovered_registry);
    assert(replay_ok);

    exchange::Logger::get_instance().set_level(exchange::LogLevel::INFO);
    LOG_INFO("WAL replay successful.");

    LOG_INFO("Original AAPL order book:");
    registry.get_order_book("AAPL")->print_book();

    LOG_INFO("Recovered AAPL order book:");
    recovered_registry.get_order_book("AAPL")->print_book();

    LOG_INFO("Original MSFT order book:");
    registry.get_order_book("MSFT")->print_book();

    LOG_INFO("Recovered MSFT order book:");
    recovered_registry.get_order_book("MSFT")->print_book();

    std::filesystem::remove(wal_path, ec);
    LOG_INFO("Demo completed successfully.");
}

int main() {
    run_multi_client_demo();
    return 0;
}
