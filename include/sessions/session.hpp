#pragma once

#include "utils/types.hpp"
#include <cstdint>
#include <vector>

class Session {
public:
    Session(int fd = -1, ClientID serverClientID = 0)
        : recvBuffer(), sendBuffer(), fd(fd), serverClientID_(serverClientID),
          serverSqn_(0), clientSqn_(0), authenticated_(false) {}

    void reset() {
        recvBuffer.clear();
        sendBuffer.clear();
        fd = -1;
        serverClientID_ = 0;
        serverSqn_ = 0;
        clientSqn_ = 0;
        authenticated_ = false;
        executionCounter_ = 0;
    }

    void clearBuffers() {
        recvBuffer.clear();
        sendBuffer.clear();
    }

    TradeID getNextExeID() { return ++executionCounter_; }
    std::uint32_t getNextServerSqn() { return ++serverSqn_; }
    std::uint32_t getNextClientSqn() { return ++clientSqn_; }

    std::vector<std::uint8_t> recvBuffer;
    std::vector<std::uint8_t> sendBuffer;

    int fd;

    constexpr ClientID getClientID() const { return serverClientID_; }
    constexpr std::uint32_t getServerSqn() const { return serverSqn_; }
    constexpr std::uint32_t getClientSqn() const { return clientSqn_; }
    constexpr TradeID getTradeID() const { return executionCounter_; }
    constexpr bool isAuthenticated() const { return authenticated_; }

private:
    std::uint64_t serverClientID_;
    std::uint32_t serverSqn_;
    std::uint32_t clientSqn_;
    bool authenticated_;
    TradeID executionCounter_;
};
