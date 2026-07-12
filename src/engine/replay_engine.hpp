#pragma once

#include <string>

namespace exchange {

class BookRegistry;

class ReplayEngine {
public:
    // Read the WAL log file and replay state-changing operations to reconstruct the exchange state
    static bool replay(const std::string& log_filepath, BookRegistry& registry);
};

} // namespace exchange
