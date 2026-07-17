#pragma once

#include <atomic>
#include <optional>
#include <cstddef>
#include <utility>

namespace exchange {

/**
 * @brief A high-performance, lock-free Multi-Producer Single-Consumer (MPSC) Queue.
 *
 * This queue uses a linked-list structure with a sentinel stub node to support:
 * - Concurrent non-blocking pushes from multiple producers.
 * - Single-threaded dequeues by a single consumer.
 * - Lock-free operation using atomic exchange and release-acquire memory barriers.
 *
 * Concurrency analysis:
 * 1. Producers:
 *    Multiple producers concurrently call `enqueue()`. They atomically exchange the
 *    `head_` pointer to point to their new node. The exchange operation is serialized
 *    and thread-safe. Once the exchange succeeds, the producer links their node as the
 *    successor of the previous head (`prev_head->next`). This store-release synchronizes
 *    with the consumer's load-acquire of the next node.
 * 2. Consumer:
 *    The single consumer thread calls `dequeue()`. It reads from the `tail_` node.
 *    Because `tail_` is only mutated/accessed by the single consumer thread, no locks
 *    are needed to access it. If `tail_->next` is not null, the consumer safely moves the
 *    data from the next node and promotes the next node to be the new stub.
 */
template <typename T>
class MPSCQueue {
private:
    struct Node {
        T data;
        std::atomic<Node*> next{nullptr};

        Node() = default;
        explicit Node(T&& val) : data(std::move(val)) {}
        explicit Node(const T& val) : data(val) {}
    };

    // Align indices and pointers to separate cache lines (typically 64 bytes)
    // to prevent false sharing between concurrent producer and consumer threads.
    alignas(64) std::atomic<Node*> head_;
    alignas(64) Node* tail_;
    alignas(64) std::atomic<bool> closed_{false};

public:
    MPSCQueue() {
        Node* stub = new Node();
        head_.store(stub, std::memory_order_relaxed);
        tail_ = stub;
    }

    ~MPSCQueue() {
        close();
        // Drain and deallocate all remaining elements in the queue
        while (auto item = dequeue()) {
            // Keep dequeuing to free memory
        }
        delete tail_; // Delete final remaining sentinel stub
    }

    // Disable copy constructor and assignment
    MPSCQueue(const MPSCQueue&) = delete;
    MPSCQueue& operator=(const MPSCQueue&) = delete;

    /**
     * @brief Pushes a value to the queue using rvalue reference (move semantics).
     * @return true if successfully enqueued, false if the queue is closed.
     */
    bool enqueue(T&& val) {
        if (closed_.load(std::memory_order_relaxed)) {
            return false;
        }
        Node* new_node = new Node(std::move(val));
        // Atomic exchange establishes the new head. Multiple producers can call this concurrently safely.
        Node* prev_head = head_.exchange(new_node, std::memory_order_acq_rel);
        // Link the previous head to the new node so that the consumer can see it.
        prev_head->next.store(new_node, std::memory_order_release);
        return true;
    }

    /**
     * @brief Pushes a value to the queue using const lvalue reference.
     * @return true if successfully enqueued, false if the queue is closed.
     */
    bool enqueue(const T& val) {
        if (closed_.load(std::memory_order_relaxed)) {
            return false;
        }
        Node* new_node = new Node(val);
        Node* prev_head = head_.exchange(new_node, std::memory_order_acq_rel);
        prev_head->next.store(new_node, std::memory_order_release);
        return true;
    }

    /**
     * @brief Pops a value from the queue. Called ONLY by the single consumer thread.
     * @return std::optional containing the popped item, or std::nullopt if the queue is empty.
     */
    std::optional<T> dequeue() {
        Node* tail = tail_;
        Node* next = tail->next.load(std::memory_order_acquire);

        if (next == nullptr) {
            // Queue is either empty or a producer has exchanged head_ but has not yet linked prev_head->next.
            return std::nullopt;
        }

        // Move the data from the next node. The next node becomes the new sentinel stub.
        T val = std::move(next->data);
        tail_ = next;
        delete tail; // Free the old sentinel stub
        return val;
    }

    /**
     * @brief Gracefully closes the queue. Prevents any new elements from being enqueued.
     */
    void close() {
        closed_.store(true, std::memory_order_relaxed);
    }

    /**
     * @brief Checks if the queue is empty. Called ONLY by the consumer thread.
     */
    bool empty() const {
        return tail_->next.load(std::memory_order_relaxed) == nullptr;
    }

    // --- SPSC Compatibility Wrappers to prevent changing existing calling logic ---
    bool write(const T& val) {
        return enqueue(val);
    }

    bool write(T&& val) {
        return enqueue(std::move(val));
    }

    std::optional<T> read() {
        return dequeue();
    }
};

} // namespace exchange
