#pragma once

#include "types.hpp"
#include <string>

namespace exchange {
namespace time_utils {

Timestamp get_current_time_ns();
std::string format_timestamp(Timestamp ts);

}
}
