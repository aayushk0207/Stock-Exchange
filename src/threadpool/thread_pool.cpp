#include "thread_pool.hpp"
#include "../registry/book_registry.hpp"
#include "../engine/order_book.hpp"
#include "../common/time_utils.hpp"
#include "../logger/logger.hpp"

#include "../pipeline/wal.hpp"
#include "../pipeline/execution_pipeline.hpp"

namespace exchange {

ThreadPool::ThreadPool(BookRegistry& registry, size_t num_threads, WAL* wal, ExecutionPipeline* pipeline)
    : registry_(registry), num_threads_(num_threads), wal_(wal), pipeline_(pipeline) {}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::start() {
    if (!workers_.empty()) {
        LOG_WARN("ThreadPool already started.");
        return;
    }

    LOG_INFO("Starting ThreadPool with " + std::to_string(num_threads_) + " workers.");
    workers_.reserve(num_threads_);
    for (size_t i = 0; i < num_threads_; ++i) {
        workers_.emplace_back(&ThreadPool::worker_loop, this);
    }
}

void ThreadPool::shutdown() {
    if (stop_.exchange(true)) {
        return;
    }

    LOG_INFO("Shutting down ThreadPool.");
    task_queue_.close();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
    LOG_INFO("ThreadPool shutdown complete.");
}

bool ThreadPool::submit(Task task) {
    if (stop_) {
        LOG_WARN("ThreadPool is stopping. Task submission rejected.");
        return false;
    }
    return task_queue_.enqueue(std::move(task));
}

void ThreadPool::worker_loop() {
    LOG_INFO("Worker thread started.");
    while (true) {
        auto task_opt = task_queue_.dequeue();
        if (!task_opt.has_value()) {
            break; // Shutdown signal received
        }

        Task task = std::move(task_opt.value());
        std::string type_str = "UNKNOWN";
        if (task.type == Task::Type::Submit) type_str = "SUBMIT";
        else if (task.type == Task::Type::Cancel) type_str = "CANCEL";
        else if (task.type == Task::Type::Modify) type_str = "MODIFY";
        LOG_INFO("[WORKER] Processing " + type_str + " request for " + task.symbol);

        try {
            // Resolve the correct OrderBook (creates if not exists, thread-safe)
            OrderBook* book = registry_.get_order_book(task.symbol);

            MatchResult result;

            if (wal_) {
                if (task.type == Task::Type::Submit) {
                    wal_->log_order(std::get<Order>(task.request));
                } else if (task.type == Task::Type::Cancel) {
                    wal_->log_cancel(std::get<CancelRequest>(task.request));
                } else if (task.type == Task::Type::Modify) {
                    wal_->log_modify(std::get<ModifyRequest>(task.request));
                }
            }

            if (task.type == Task::Type::Submit) {
                const auto& order = std::get<Order>(task.request);
                result = book->submitOrder(order);
            } else if (task.type == Task::Type::Cancel) {
                const auto& req = std::get<CancelRequest>(task.request);
                book->cancelOrder(req.order_id, result);
            } else if (task.type == Task::Type::Modify) {
                const auto& req = std::get<ModifyRequest>(task.request);
                book->modifyOrder(req, result);
            }

            if (wal_) {
                for (const auto& fill : result.fills) {
                    wal_->log_fill(fill);
                }
            }

            OrderID completed_order_id = 0;
            if (task.type == Task::Type::Submit) {
                completed_order_id = std::get<Order>(task.request).order_id;
            } else if (task.type == Task::Type::Cancel) {
                completed_order_id = std::get<CancelRequest>(task.request).order_id;
            } else if (task.type == Task::Type::Modify) {
                completed_order_id = std::get<ModifyRequest>(task.request).order_id;
            }
            LOG_INFO("[WORKER] Match complete for Order " + std::to_string(completed_order_id));

            // Invoke client callback or publish results to ExecutionPipeline
            if (pipeline_) {
                if (!result.execution_reports.empty()) {
                    LOG_INFO("[EXECUTION PIPELINE] Queued " + std::to_string(result.execution_reports.size()) + " execution reports");
                }
                for (const auto& report : result.execution_reports) {
                    pipeline_->publish_execution_report(report);
                }
                for (const auto& fill : result.fills) {
                    pipeline_->publish_trade(Trade{fill.trade_id, fill.symbol, fill.price, fill.quantity, fill.timestamp});
                }
            } else if (task.callback) {
                task.callback(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Exception caught in worker task thread: " + std::string(e.what()));
        } catch (...) {
            LOG_ERROR("Unknown exception caught in worker task thread.");
        }
    }
    LOG_INFO("Worker thread exiting.");
}

} // namespace exchange
