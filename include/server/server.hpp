#pragma once

#include <arpa/inet.h>
#include <iostream>
#include <netdb.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string>

#include "protocol/protocolHandler.hpp"
#include "protocol/sendFunctions.hpp"
#include "server/clients.hpp"

class Server {
public:
    Server(const std::string& address = "127.0.0.1", uint16_t port = 12345,
           int n_events = 64)
        : handler_(ProtocolHandler(SendFunctions::socketSend())), address_(address),
          port_(port), n_events_(n_events) {
        std::cout << "Server initialized on " << address_ << ":" << port_ << std::endl;
    }

    void start() {
        setupListenSocket_();
        setupEpoll_();

        running_ = true;
        std::vector<epoll_event> eventList(n_events_);

        while (running_) {
            int n = epoll_wait(epollFD_, eventList.data(), n_events_, -1);
            if (n < 0) {
                if (errno == EINTR) continue;
                perror("epoll_wait");
                break;
            }
            for (int i = 0; i < n; i++) {
                handleEvent_(eventList[i]);
            }
        }
    }

    void stop() {
        running_ = false;
        std::cout << "Server stopping..." << std::endl;
        close(listenFD_);
        close(epollFD_);
    }

private:
    ProtocolHandler handler_;
    std::string address_;
    uint16_t port_;
    const int n_events_;
    int listenFD_{-1};
    int epollFD_{-1};
    bool running_{false};

    void setupListenSocket_();
    void setupEpoll_();
    void handleEvent_(epoll_event& ev);
};