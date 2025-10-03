#pragma once

#include "protocol/protocolHandler.hpp"
#include "server/connectionManager.hpp"

#include <chrono>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

class Server {
public:
    Server(uint16_t port, SessionManager& sm, ProtocolHandler& handler,
           std::shared_ptr<Logger<>> logger = nullptr)
        : port_(port), sessionManager_(sm), connManager_(sm), handler_(handler),
          logger_(logger) {}

    bool start(uint16_t port);
    void run();
    void stop();

private:
    void acceptConnections();
    void handleRead(int fd);
    void handleWrite(int fd);
    void handleDisconnect(int fd);
    void scheduleWrite(int fd);

    int createListenSocket(uint16_t port);

    void checkHeartbeats_();

    uint16_t port_;
    int listenFd_{-1};
    int epollFd_{-1};
    bool running_{false};

    SessionManager& sessionManager_;
    ConnectionManager connManager_;
    ProtocolHandler& handler_;
    std::shared_ptr<Logger<>> logger_;

    static const int MAX_EVENTS{128};
    const std::chrono::seconds HEARTBEAT_TIMEOUT_SECONDS{10};
};
