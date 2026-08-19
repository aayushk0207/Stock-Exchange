#pragma once

#include "../common/types.hpp"
#include <variant>

namespace exchange {

struct Task {
    enum class Type {
        Submit,
        Cancel,
        Modify
    };

    Type type;
    std::variant<Order, CancelRequest, ModifyRequest> request;
    Symbol symbol;
};

}
