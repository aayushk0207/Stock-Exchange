#pragma once

#include <unordered_map>
#include <memory>
#include <mutex>
#include <cstdint>

namespace exchange {

class Session;

// Thread-safe manager to track active client sessions indexed by ClientID
class ConnectionManager {
public:
    static ConnectionManager& get_instance() {
        static ConnectionManager instance;
        return instance;
    }

    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;

    void add_session(uint32_t client_id, std::shared_ptr<Session> session) {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_[client_id] = session;
    }

    void remove_session(uint32_t client_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_.erase(client_id);
    }

    std::shared_ptr<Session> get_session(uint32_t client_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(client_id);
        if (it != sessions_.end()) {
            return it->second;
        }
        return nullptr;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_.clear();
    }

private:
    ConnectionManager() = default;
    ~ConnectionManager() = default;

    std::unordered_map<uint32_t, std::shared_ptr<Session>> sessions_;
    mutable std::mutex mutex_;
};

}


