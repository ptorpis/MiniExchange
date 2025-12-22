#pragma once

#include "protocol/protocolTypes.hpp"
#include "utils/types.hpp"

class Session {
public:
    Session(int fileDescriptor = -1, ClientID serverClientID = ClientID{0})
        : recvBuffer(), sendBuffer(), fd(fileDescriptor), serverClientID_(serverClientID),
          serverSqn_(0), clientSqn_(0), authenticated_(false) {
        recvBuffer.reserve(4 * 1024);
        sendBuffer.reserve(4 * 1024);
    }

    void reset() {
        recvBuffer.clear();
        sendBuffer.clear();
        fd = -1;
        serverClientID_ = ClientID{0};
        serverSqn_ = ServerSqn32{0};
        clientSqn_ = ClientSqn32{0};
        authenticated_ = false;
        executionCounter_ = TradeID{0};
    }

    void clearBuffers() {
        recvBuffer.clear();
        sendBuffer.clear();
    }

    TradeID getNextExeID() { return ++executionCounter_; }
    ServerSqn32 getNextServerSqn() { return ++serverSqn_; }
    ClientSqn32 getNextClientSqn() { return ++clientSqn_; }

    MessageBuffer recvBuffer;
    MessageBuffer sendBuffer;

    int fd;

    constexpr ClientID getClientID() const { return serverClientID_; }
    constexpr ServerSqn32 getServerSqn() const { return serverSqn_; }
    constexpr ClientSqn32 getClientSqn() const { return clientSqn_; }
    constexpr TradeID getTradeID() const { return executionCounter_; }
    constexpr bool isAuthenticated() const { return authenticated_; }

    void authenticate() noexcept { authenticated_ = true; }
    void logout() noexcept { authenticated_ = false; }

private:
    ClientID serverClientID_;
    ServerSqn32 serverSqn_;
    ClientSqn32 clientSqn_;
    bool authenticated_;
    TradeID executionCounter_{0};
};
