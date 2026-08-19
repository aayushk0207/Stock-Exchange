#pragma once

#include <atomic>
#include <optional>
#include <cstddef>
#include <utility>

namespace exchange {

template <typename T>
class MPSCQueue {
private:
    struct Node {
        T data;
        std::atomic<Node*> next{nullptr};

        Node() = default;
        explicit Node(T val) : data(std::move(val)) {}
    };

    alignas(64) std::atomic<Node*> tail_;
    alignas(64) Node* head_;
    alignas(64) std::atomic<bool> closed_{false};

public:
    MPSCQueue() {
        Node* stub = new Node();
        tail_.store(stub);
        head_ = stub;
    }

    ~MPSCQueue() {
        close();
        while (auto item = dequeue()) {}
        delete head_;
    }

    MPSCQueue(const MPSCQueue&) = delete;
    MPSCQueue& operator=(const MPSCQueue&) = delete;

    bool enqueue(T val) {
        if (closed_.load()) {
            return false;
        }
        Node* new_node = new Node(std::move(val));
        Node* prev_tail = tail_.exchange(new_node);
        prev_tail->next.store(new_node);
        return true;
    }

    std::optional<T> dequeue() {
        Node* head = head_;
        Node* next = head->next.load();

        if (next == nullptr) {
            return std::nullopt;
        }

        T val = std::move(next->data);
        head_ = next;
        delete head;
        return val;
    }

    void close() {
        closed_.store(true);
    }

    bool empty() const {
        return head_->next.load() == nullptr;
    }
};

}
