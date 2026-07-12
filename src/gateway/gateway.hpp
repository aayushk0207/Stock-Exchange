#pragma once

#include "../common/types.hpp"
#include <memory>

namespace exchange {

// Forward declaration of downstream components
class ThreadPool;

class Gateway {
public:
    Gateway(ThreadPool& thread_pool);
    ~Gateway();

    void start();
    void stop();

    // Client/External interface to submit orders
    void submit_order(const Order& order);

private:
    ThreadPool& thread_pool_;
    bool running_{false};
};

} // namespace exchange
