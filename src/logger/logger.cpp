#include "logger.hpp"

namespace exchange {

Logger& Logger::get_instance() {
    static Logger instance;
    return instance;
}

void Logger::set_level(LogLevel level) {
    level_.store(level, std::memory_order_relaxed);
}

LogLevel Logger::get_level() const {
    return level_.load(std::memory_order_relaxed);
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < level_.load(std::memory_order_relaxed)) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (level < level_.load(std::memory_order_relaxed)) {
        return;
    }

    std::string level_str;
    switch (level) {
        case LogLevel::DEBUG: level_str = "DEBUG"; break;
        case LogLevel::INFO:  level_str = "INFO "; break;
        case LogLevel::WARN:  level_str = "WARN "; break;
        case LogLevel::ERROR: level_str = "ERROR"; break;
    }

    std::string time_str = time_utils::format_timestamp(time_utils::get_current_time_ns());

    std::cout << "[" << time_str << "] [" << level_str << "] " << message << std::endl;
}

}
