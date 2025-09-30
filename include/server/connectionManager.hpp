#pragma once

#include <chrono>
#include <string>

#include "auth/sessionManager.hpp"

class Connection {
public:
    Connection(int fd, uint16_t port, const std::string& ip)
        : fd_(fd), clientPort_(port), clientIP_(ip) {}

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) = default;
    Connection& operator=(Connection&&) = default;

    int fd() const { return fd_; }
    const std::string& clientIP() const { return clientIP_; }
    uint16_t clientPort() const { return clientPort_; }

    void touch() { lastActive_ = std::chrono::steady_clock::now(); }
    std::chrono::steady_clock::time_point lastActive() const { return lastActive_; }

    bool isWritable{false};

private:
    int fd_;
    uint16_t clientPort_{0};
    std::string clientIP_;
    std::chrono::steady_clock::time_point lastActive_{std::chrono::steady_clock::now()};
};

class ConnectionManager {
public:
    explicit ConnectionManager(SessionManager& sm) : sessionManager_(sm) {}

    Connection& addConnection(uint16_t port, const std::string& ip, int fd) {
        sessionManager_.createSession(fd);
        auto [it, _] = connections_.emplace(fd, Connection(fd, port, ip));
        return it->second;
    }

    void removeConnection(int fd) {
        sessionManager_.removeSession(fd);
        connections_.erase(fd);
    }

    Connection* getConnection(int fd) {
        auto it = connections_.find(fd);
        return (it != connections_.end()) ? &it->second : nullptr;
    }

private:
    SessionManager& sessionManager_;
    std::unordered_map<int, Connection> connections_;
};
