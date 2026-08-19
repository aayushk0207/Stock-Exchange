#pragma once

#define ASIO_STANDALONE 1
#include <asio.hpp>
#include <memory>
#include <atomic>
#include <queue>
#include <mutex>
#include <vector>
#include "../common/types.hpp"
#include "../threadpool/thread_pool.hpp"
#include "../engine/risk_checker.hpp"

#include "binary_serializer.hpp"
#include "connection_manager.hpp"
#include "../common/time_utils.hpp"

namespace exchange {

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(asio::ip::tcp::socket socket, ThreadPool& pool)
        : socket_(std::move(socket)), pool_(pool) {
        static std::atomic<uint32_t> next_client_id{1};
        client_id_ = next_client_id.fetch_add(1);
    }

    ~Session() {
        close();
    }

    asio::ip::tcp::socket& socket() {
        return socket_;
    }

    uint32_t client_id() const {
        return client_id_;
    }

    void start() {
        LOG_INFO("Session started for client ID: " + std::to_string(client_id_));
        ConnectionManager::get_instance().add_session(client_id_, shared_from_this());
        read_header();
    }

    void close() {
        bool expected = true;
        if (active_.compare_exchange_strong(expected, false)) {
            ConnectionManager::get_instance().remove_session(client_id_);
            std::error_code ec;
            socket_.close(ec);
            LOG_INFO("Session closed for client ID: " + std::to_string(client_id_));
        }
    }

    // Thread-safe asynchronous send of an execution report to the client
    void send_execution_report(const ExecutionReport& report) {
        std::string status_str = to_string(report.status);
        LOG_INFO("[EXECUTION] Order " + std::to_string(report.order_id) + " -> " + status_str);
        if (report.status == OrderStatus::Filled || report.status == OrderStatus::PartiallyFilled) {
            LOG_INFO("Executed " + std::to_string(report.last_qty) + " shares @ " + std::to_string(report.last_px));
        } else if (report.status == OrderStatus::Rejected) {
            LOG_INFO("Reject Reason: " + report.reject_reason);
        }
        auto self = shared_from_this();
        asio::post(socket_.get_executor(), [this, self, report]() {
            NetExecutionReport net_report = BinarySerializer::to_net(report);
            MsgHeader header;
            header.type = static_cast<uint16_t>(MsgType::ExecutionReport);
            header.length = sizeof(NetExecutionReport);

            std::vector<char> buffer(sizeof(MsgHeader) + sizeof(NetExecutionReport));
            std::memcpy(buffer.data(), &header, sizeof(MsgHeader));
            std::memcpy(buffer.data() + sizeof(MsgHeader), &net_report, sizeof(NetExecutionReport));

            bool write_in_progress = !write_queue_.empty();
            write_queue_.push(std::move(buffer));
            LOG_INFO("[SESSION] Async write queued");

            if (!write_in_progress) {
                write_next();
            }
        });
    }

private:
    void read_header() {
        auto self = shared_from_this();
        asio::async_read(socket_, asio::buffer(&incoming_header_, sizeof(MsgHeader)),
            [this, self](std::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    read_body();
                } else {
                    close();
                }
            });
    }

    void read_body() {
        auto self = shared_from_this();
        incoming_body_.resize(incoming_header_.length);
        asio::async_read(socket_, asio::buffer(incoming_body_.data(), incoming_header_.length),
            [this, self](std::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    process_packet();
                    read_header();
                } else {
                    close();
                }
            });
    }

    void process_packet() {
        MsgType type = static_cast<MsgType>(incoming_header_.type);
        Task task;
        task.symbol = ""; // To be filled from packet

        if (type == MsgType::Order) {
            if (incoming_body_.size() != sizeof(NetOrder)) {
                LOG_ERROR("Invalid Order packet size.");
                return;
            }
            NetOrder net_order;
            std::memcpy(&net_order, incoming_body_.data(), sizeof(NetOrder));
            Order order = BinarySerializer::from_net(net_order);
            order.client_id = client_id_;

            // 1. Gateway pre-validation boundary check via RiskChecker
            std::string reject_reason;
            if (!RiskChecker::get_instance().validate_order(order, reject_reason)) {
                LOG_WARN("Risk Check failed for OrderID=" + std::to_string(order.order_id) + ": " + reject_reason);
                ExecutionReport reject_report;
                reject_report.order_id = order.order_id;
                reject_report.symbol = order.symbol;
                reject_report.side = order.side;
                reject_report.status = OrderStatus::Rejected;
                reject_report.reject_reason = reject_reason;
                reject_report.timestamp = time_utils::get_current_time_ns();
                reject_report.client_id = client_id_;
                send_execution_report(reject_report);
                return;
            }
            LOG_INFO("Risk Check passed for OrderID=" + std::to_string(order.order_id));

            task.type = Task::Type::Submit;
            task.request = order;
            task.symbol = order.symbol;
        } 
        else if (type == MsgType::Cancel) {
            if (incoming_body_.size() != sizeof(NetCancel)) {
                LOG_ERROR("Invalid Cancel packet size.");
                return;
            }
            NetCancel net_cancel;
            std::memcpy(&net_cancel, incoming_body_.data(), sizeof(NetCancel));
            CancelRequest req = BinarySerializer::from_net(net_cancel);
            req.client_id = client_id_;
            task.type = Task::Type::Cancel;
            task.request = req;
            task.symbol = req.symbol;
        } 
        else if (type == MsgType::Modify) {
            if (incoming_body_.size() != sizeof(NetModify)) {
                LOG_ERROR("Invalid Modify packet size.");
                return;
            }
            NetModify net_modify;
            std::memcpy(&net_modify, incoming_body_.data(), sizeof(NetModify));
            ModifyRequest req = BinarySerializer::from_net(net_modify);
            req.client_id = client_id_;
            task.type = Task::Type::Modify;
            task.request = req;
            task.symbol = req.symbol;
        } 
        else {
            LOG_WARN("Session received unknown packet type: " + std::to_string(static_cast<int>(type)));
            return;
        }

        // Submit unit of work to thread pool
        pool_.submit(task);
    }

    void write_next() {
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(write_queue_.front().data(), write_queue_.front().size()),
            [this, self](std::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    write_queue_.pop();
                    if (!write_queue_.empty()) {
                        write_next();
                    }
                } else {
                    close();
                }
            });
    }

    asio::ip::tcp::socket socket_;
    ThreadPool& pool_;
    uint32_t client_id_;
    std::atomic<bool> active_{true};

    MsgHeader incoming_header_;
    std::vector<char> incoming_body_;

    std::queue<std::vector<char>> write_queue_;
};

}
