#pragma once

#include "protocol/protocolHandler.hpp"
#include "sessions/sessionManager.hpp"
#include <atomic>
#include <cstdint>
#include <sys/epoll.h>

class MiniExchangeGateway {
public:
    MiniExchangeGateway(ProtocolHandler& handler, SessionManager& sm, std::uint16_t port)
        : running_(false), listenFD_(-1), epollFD_(-1), port_(port), handler_(handler),
          sessionManager_(sm) {
        setupListenSocket_();
        setupEpoll_();
        setupShutdownPipe_();
    }
    ~MiniExchangeGateway() {
        if (running_.load()) {
            stop();
        }

        if (listenFD_ >= 0) close(listenFD_);
        if (epollFD_ >= 0) close(epollFD_);
        if (shutdownPipe_[0] >= 0) close(shutdownPipe_[0]);
        if (shutdownPipe_[1] >= 0) close(shutdownPipe_[1]);
    }

    void run();
    void stop();

private:
    std::atomic<bool> running_{false};
    int shutdownPipe_[2];

    int listenFD_;
    int epollFD_;
    std::uint16_t port_;

    static constexpr int MAX_EVENTS = 128;
    epoll_event events_[MAX_EVENTS];

    ProtocolHandler& handler_;
    SessionManager& sessionManager_;

    void setupListenSocket_();
    void setupEpoll_();

    void handleAccept_();
    void handleRead_(int fd);
    void handleWrite_(int fd);
    void handleError_(int fd);

    void addToEpoll_(int fd, std::uint32_t events_);
    void modifyEpoll_(int fd, std::uint32_t events_);
    void removeFromEpoll_(int fd);

    void acceptConnection_();
    void closeConnection_(int fd);

    void setNonBlocking_(int fd);
    void setTCPNoDelay_(int fd);

    void setupShutdownPipe_();
    void shutdown_();
};
