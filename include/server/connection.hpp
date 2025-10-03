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

private:
    int fd_;
    uint16_t clientPort_{0};
    std::string clientIP_;
    std::chrono::steady_clock::time_point lastActive_{std::chrono::steady_clock::now()};
};
