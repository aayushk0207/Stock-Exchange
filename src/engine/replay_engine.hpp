#pragma once

#include <string>

namespace exchange {

class BookRegistry;

class ReplayEngine {
public:
    static bool replay(const std::string& log_filepath, BookRegistry& registry);
};

}
