#pragma once

#include "types.hpp"
#include <string>

namespace exchange {
namespace time_utils {

// Get current system time in nanoseconds since epoch
Timestamp get_current_time_ns();

// Format nanoseconds since epoch into a human-readable string: YYYY-MM-DD HH:MM:SS.ns
std::string format_timestamp(Timestamp ts);

} // namespace time_utils
} // namespace exchange
