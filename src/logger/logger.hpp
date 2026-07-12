#pragma once

#include <string>
#include <mutex>
#include <iostream>
#include <sstream>
#include "../common/time_utils.hpp"

namespace exchange {

#ifdef ERROR
#undef ERROR
#endif

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

class Logger {
public:
    static Logger& get_instance();

    // Prevent copying
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void set_level(LogLevel level);
    LogLevel get_level() const;

    // Thread-safe logging function
    void log(LogLevel level, const std::string& message);

    // Helpers
    void debug(const std::string& message) { log(LogLevel::DEBUG, message); }
    void info(const std::string& message) { log(LogLevel::INFO, message); }
    void warn(const std::string& message) { log(LogLevel::WARN, message); }
    void error(const std::string& message) { log(LogLevel::ERROR, message); }

private:
    Logger() = default;
    ~Logger() = default;

    LogLevel level_{LogLevel::INFO};
    mutable std::mutex mutex_;
};

// Convenient macros/inline wrappers for logging
#define LOG_DEBUG(msg) ::exchange::Logger::get_instance().debug(msg)
#define LOG_INFO(msg)  ::exchange::Logger::get_instance().info(msg)
#define LOG_WARN(msg)  ::exchange::Logger::get_instance().warn(msg)
#define LOG_ERROR(msg) ::exchange::Logger::get_instance().error(msg)

} // namespace exchange
