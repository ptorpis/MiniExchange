#include "server/server.hpp"

#include <fcntl.h>

void Server::setupListenSocket_() {
    struct addrinfo hints {
    }, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int status;
    if ((status = getaddrinfo(address_.c_str(), std::to_string(port_).c_str(), &hints,
                              &res)) != 0) {
        throw std::runtime_error("getaddrinfo error: " +
                                 std::string(gai_strerror(status)));
    }

    listenFD_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (listenFD_ < 0) {
        freeaddrinfo(res);
        throw std::runtime_error("socket creation failed");
    }

    int opt = 1;
    if (setsockopt(listenFD_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(listenFD_);
        freeaddrinfo(res);
        throw std::runtime_error("setsockopt failed");
    }

    if (bind(listenFD_, res->ai_addr, res->ai_addrlen) < 0) {
        close(listenFD_);
        freeaddrinfo(res);
        throw std::runtime_error("bind failed");
    }

    freeaddrinfo(res);

    if (listen(listenFD_, SOMAXCONN) < 0) {
        close(listenFD_);
        throw std::runtime_error("listen failed");
    }
}

void Server::setupEpoll_() {
    std::cout << "Starting server on " << address_ << ":" << port_ << std::endl;
    epollFD_ = epoll_create1(0);
    if (epollFD_ < 0) {
        close(listenFD_);
        throw std::runtime_error("epoll_create1 failed");
    }

    struct epoll_event event {};
    event.data.fd = listenFD_;
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epollFD_, EPOLL_CTL_ADD, listenFD_, &event) < 0) {
        close(listenFD_);
        close(epollFD_);
        throw std::runtime_error("epoll_ctl failed");
    }
}

void Server::handleEvent_(epoll_event& ev) {
    if (ev.data.fd == listenFD_) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientFD = accept(listenFD_, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientFD < 0) {
            perror("accept");
            return;
        }

        int flags = fcntl(clientFD, F_GETFL, 0);
        if (flags == -1) flags = 0;
        if (fcntl(clientFD, F_SETFL, flags | O_NONBLOCK) == -1) {
            perror("fcntl");
            close(clientFD);
            return;
        }

        struct epoll_event event {};
        event.data.fd = clientFD;
        event.events = EPOLLIN | EPOLLET;
        if (epoll_ctl(epollFD_, EPOLL_CTL_ADD, clientFD, &event) < 0) {
            perror("epoll_ctl");
            close(clientFD);
            return;
        }

        Session& session = handler_.createSession(clientFD);
        std::cout << "New connection accepted, fd: " << clientFD << std::endl;

        if (!session.recvBuffer.empty()) {
            std::cout << "Processing buffered data for fd: " << clientFD << std::endl;
            handler_.onMessage(clientFD);
        } else {
            std::cout << "No buffered data for fd: " << clientFD << std::endl;
        }

    } else {
        if (ev.events & (EPOLLERR | EPOLLHUP)) {
            handler_.onDisconnect(ev.data.fd);
            close(ev.data.fd);
            return;
        }

        if (ev.events & EPOLLIN) {
            // read data into session recvbuffer
            Session* session = handler_.getSession(ev.data.fd);
            if (!session) {
                std::cerr << "No session found for fd: " << ev.data.fd << std::endl;
                close(ev.data.fd);
                return;
            }

            std::vector<uint8_t> buffer(4096);
            ssize_t count;
            while ((count = recv(ev.data.fd, buffer.data(), buffer.size(), 0)) > 0) {
                session->recvBuffer.insert(session->recvBuffer.end(), buffer.data(),
                                           buffer.data() + count);
            }
            if (count == -1 && errno != EAGAIN) {
                perror("recv");
                handler_.onDisconnect(ev.data.fd);
                close(ev.data.fd);
                return;
            } else if (count == 0) {
                std::cout << "Client disconnected, fd: " << ev.data.fd << std::endl;
                handler_.onDisconnect(ev.data.fd);
                close(ev.data.fd);
                return;
            }

            handler_.onMessage(ev.data.fd);
        }
    }
}