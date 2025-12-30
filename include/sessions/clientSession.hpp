#pragma once

#include "utils/types.hpp"
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

struct ClientSession {
    int sockfd{-1};
    std::string host;
    std::uint16_t port;
    std::atomic<bool> connected{false};

    std::vector<std::byte> recvBuffer;
    std::vector<std::byte> sendBuffer;

    std::mutex sendMutex;

    ClientSqn32 clientSqn{0};
    ServerSqn32 serverSqn{0};

    ClientID serverClientID{0};
    ClientOrderID orderIDCounter{0};

    ClientSession(std::string h, std::uint16_t p) : host(std::move(h)), port(p) {
        recvBuffer.reserve(4 * 1024);
        sendBuffer.reserve(4 * 1024);
    }

    ClientOrderID getNextOrderID() { return ++orderIDCounter; }

    bool isConnected() const {
        return sockfd >= 0 && connected.load(std::memory_order_acquire);
    }
};
