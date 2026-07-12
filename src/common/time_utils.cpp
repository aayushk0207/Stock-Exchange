#include "time_utils.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace exchange {
namespace time_utils {

Timestamp get_current_time_ns() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

std::string format_timestamp(Timestamp ts) {
    auto nanos = ts % 1000000000;
    std::time_t secs = ts / 1000000000;

    std::tm time_info;
#if defined(_MSC_VER) || defined(__MINGW32__)
    // Thread-safe time conversion on Windows
    localtime_s(&time_info, &secs);
#else
    localtime_r(&secs, &time_info);
#endif

    std::ostringstream oss;
    oss << std::put_time(&time_info, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setw(9) << std::setfill('0') << nanos;
    return oss.str();
}

} // namespace time_utils
} // namespace exchange
