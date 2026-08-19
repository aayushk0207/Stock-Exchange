#pragma once

#define ASIO_STANDALONE 1
#include <asio.hpp>
#include <memory>
#include "../logger/logger.hpp"
#include "../threadpool/thread_pool.hpp"
#include "session.hpp"

namespace exchange {

class GatewayServer {
public:
    GatewayServer(asio::io_context& io_context, uint16_t port, ThreadPool& pool)
        : io_context_(io_context),
          acceptor_(io_context),
          pool_(pool) {
        acceptor_.open(asio::ip::tcp::v4());
        acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
        acceptor_.bind(asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port));
        acceptor_.listen();
    }

    ~GatewayServer() = default;

    void start() {
        LOG_INFO("GatewayServer listening on port " + std::to_string(acceptor_.local_endpoint().port()));
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](std::error_code ec, asio::ip::tcp::socket socket) {
                if (!ec) {
                    auto session = std::make_shared<Session>(std::move(socket), pool_);
                    session->start();
                } else {
                    LOG_ERROR("GatewayServer accept error: " + ec.message());
                }
                do_accept();
            });
    }

    asio::io_context& io_context_;
    asio::ip::tcp::acceptor acceptor_;
    ThreadPool& pool_;
};

}
