#pragma once

#include "events/eventBus.hpp"
#include "protocol/protocolHandler.hpp"
#include "server/connection.hpp"

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
           std::shared_ptr<EventBus> evBus = nullptr)
        : port_(port), sessionManager_(sm), handler_(handler), evBus_(evBus) {}

    bool start(uint16_t port);
    void run();
    void stop();

private:
    void acceptConnections();
    void handleRead(int fd);
    void handleWrite(int fd);
    void handleDisconnect(int fd);
    void scheduleWrite(int fd);

    Connection& addConnection(uint16_t port, const std::string& ip, int fd);
    void removeConnection(int fd);
    Connection* getConnection(int fd);

    int createListenSocket(uint16_t port);

    void checkHeartbeats_();

    uint16_t port_;
    int listenFd_{-1};
    int epollFd_{-1};
    bool running_{false};

    SessionManager& sessionManager_;
    ProtocolHandler& handler_;
    std::shared_ptr<EventBus> evBus_;
    std::unordered_map<int, Connection> connections_;

    std::chrono::steady_clock::time_point lastScreenUpdate_;

    static const int MAX_EVENTS{128};
    const std::chrono::seconds HEARTBEAT_TIMEOUT_SECONDS{1000};
};
