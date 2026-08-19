#pragma once

#include <cstddef>
#include "types.hpp"

namespace exchange {
namespace constants {

constexpr size_t DEFAULT_THREAD_POOL_SIZE = 4;
constexpr size_t MAX_QUEUE_CAPACITY = 65536;

constexpr Price MIN_PRICE = 1;
constexpr Price MAX_PRICE = 99999999;
constexpr Quantity MAX_QUANTITY = 10000000;

}
}
