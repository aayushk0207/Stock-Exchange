#pragma once

#include "../common/types.hpp"
#include <functional>
#include <variant>

namespace exchange {

// Represents a single execution request unit for the ThreadPool.
struct Task {
    enum class Type {
        Submit,
        Cancel,
        Modify
    };

    Type type;
    std::variant<Order, CancelRequest, ModifyRequest> request;
    Symbol symbol;
    std::function<void(const MatchResult&)> callback;

};

} // namespace exchange
