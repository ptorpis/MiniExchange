#pragma once

#include "utils/types.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

struct Session {

    int FD;
    uint32_t serverSqn{0};
    uint32_t clientSqn{0};

    const std::chrono::steady_clock::time_point created{std::chrono::steady_clock::now()};
    bool authenticated{false};

    std::vector<uint8_t> recvBuffer;
    std::vector<uint8_t> sendBuffer;

    uint64_t serverClientID{0};

    explicit Session(int fd) : FD(fd), created(std::chrono::steady_clock::now()) {}

    void reset() {
        recvBuffer.clear();
        sendBuffer.clear();
        authenticated = false;
        serverSqn = 0;
        serverClientID = 0;
        exeID_ = 0;
    }

    void reserveBuffer() {
        recvBuffer.reserve(4 * 1024);
        sendBuffer.reserve(4 * 1024);
    }

    void clearBuffers() {
        recvBuffer.clear();
        sendBuffer.clear();
    }

    TradeID getNextExeID() { return ++exeID_; }

private:
    TradeID exeID_{0};
};

struct OutstandingOrder {
    std::chrono::steady_clock::time_point created;
    OrderID id;
    Qty qty;
    Price price;
};

class ClientSession {
public:
    uint32_t serverSqn{0};
    uint32_t clientSqn{0};

    std::chrono::steady_clock::time_point lastHeartBeat{std::chrono::steady_clock::now()};
    bool authenticated{false};

    std::vector<uint8_t> recvBuffer;
    std::vector<uint8_t> sendBuffer;

    uint64_t serverClientID{0};

    void reset() {
        recvBuffer.clear();
        sendBuffer.clear();
        authenticated = false;
        serverSqn = 0;
        serverClientID = 0;
        exeID = 0;
    }

    void reserve() {
        recvBuffer.reserve(8 * 1024);
        sendBuffer.reserve(8 * 1024);
    }

    void updateHeartbeat() { lastHeartBeat = std::chrono::steady_clock::now(); }

    TradeID exeID{0};
};
