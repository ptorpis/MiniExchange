#include "gateway/gateway.hpp"
#include "sessions/session.hpp"

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

void MiniExchangeGateway::run() {
    running_.store(true, std::memory_order_relaxed);

    while (running_.load(std::memory_order_acquire)) {
        int nfds = epoll_wait(epollFD_, events_, MAX_EVENTS, -1);

        if (nfds < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        for (int i{}; i < nfds; ++i) {
            int fd = events_[i].data.fd;
            std::uint32_t ev = events_[i].events;

            if (fd == shutdownPipe_[0]) {
                shutdown_();
                return;
            }

            if (ev & (EPOLLERR | EPOLLHUP)) {
                handleError_(fd);
                continue;
            }

            if (fd == listenFD_) {
                if (ev & EPOLLIN) {
                    acceptConnection_();
                }
                continue;
            }

            if (ev & EPOLLIN) {
                handleRead_(fd);
            }
            if (ev & EPOLLOUT) {
                handleWrite_(fd);
            }
        }
    }
    shutdown_();
}

void MiniExchangeGateway::handleRead_(int fd) {
    Session* session = sessionManager_.getSession(fd);
    if (!session) {
        closeConnection_(fd);
    }

    while (true) {
        char buffer[4096];
        ssize_t n = ::read(fd, buffer, sizeof(buffer));

        if (n > 0) {
            session->recvBuffer.insert(session->recvBuffer.end(),
                                       reinterpret_cast<std::byte*>(buffer),
                                       reinterpret_cast<std::byte*>(buffer + n));

        } else if (n == 0) {
            closeConnection_(fd);
            return;
        } else {
            if (errno == EAGAIN) {
                break;
            } else if (errno == EINTR) {
                continue;
            } else {
                handleError_(fd);
                return;
            }
        }
    }

    handler_.onMessage(fd);

    if (handler_.getDirtyFDs().count(fd) > 0) {
        modifyEpoll_(fd, EPOLLIN | EPOLLOUT | EPOLLET);
    }
}

void MiniExchangeGateway::handleWrite_(int fd) {
    Session* session = sessionManager_.getSession(fd);
    if (!session) {
        closeConnection_(fd);
        return;
    }

    while (!session->sendBuffer.empty()) {
        ssize_t written =
            write(fd, session->sendBuffer.data(), session->sendBuffer.size());

        if (written > 0) {
            session->sendBuffer.erase(session->sendBuffer.begin(),
                                      session->sendBuffer.begin() + written);
        } else if (written < 0) {
            if (errno == EAGAIN) {
                return;
            } else if (errno == EINTR) {
                continue;
            } else {
                handleError_(fd);
            }
        }
    }

    handler_.clearDirtyFD(fd);

    modifyEpoll_(fd, EPOLLIN | EPOLLET);
}

void MiniExchangeGateway::acceptConnection_() {
    while (true) {
        sockaddr_in clientAddr;
        socklen_t addrLen = sizeof(clientAddr);

        int clientFD =
            accept(listenFD_, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);

        if (clientFD < 0) {
            if (errno == EAGAIN) {
                break;
            } else if (errno == EINTR) {
                continue;
            } else {
                break;
            }
        }

        setNonBlocking_(clientFD);
        setTCPNoDelay_(clientFD);

        sessionManager_.createSession(clientFD);
        addToEpoll_(clientFD, EPOLLIN | EPOLLET);
    }
}

void MiniExchangeGateway::setupShutdownPipe_() {
    if (pipe2(shutdownPipe_, O_NONBLOCK) < 0) {
        throw std::runtime_error("Failed to create shutdown pipe");
    }

    epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = shutdownPipe_[0];
    epoll_ctl(epollFD_, EPOLL_CTL_ADD, shutdownPipe_[0], &ev);
}

void MiniExchangeGateway::stop() {
    running_.store(false, std::memory_order_release);

    char dummy = 1;
    write(shutdownPipe_[1], &dummy, 1);
}

void MiniExchangeGateway::shutdown_() {
    if (listenFD_ >= 0) {
        close(listenFD_);
        listenFD_ = -1;
    }

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);

    while (std::chrono::steady_clock::now() < deadline) {
        bool allEmpty = true;

        for (int fd : handler_.getDirtyFDs()) {
            Session* session = sessionManager_.getSession(fd);
            if (session && !session->sendBuffer.empty()) {
                allEmpty = false;
                handleWrite_(fd);
            }
        }

        if (allEmpty) break;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    for (auto& [fd, session] : sessionManager_.getSessions()) {
        close(fd);
    }

    close(epollFD_);
    close(shutdownPipe_[0]);
    close(shutdownPipe_[1]);
}

void MiniExchangeGateway::handleError_(int fd) {
    int error = 0;
    socklen_t errlen = sizeof(error);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &errlen);

    closeConnection_(fd);
}

void MiniExchangeGateway::closeConnection_(int fd) {
    removeFromEpoll_(fd);
    sessionManager_.removeSession(fd);
    handler_.clearDirtyFD(fd);
    close(fd);
}

void MiniExchangeGateway::addToEpoll_(int fd, std::uint32_t events) {
    epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(epollFD_, EPOLL_CTL_ADD, fd, &ev) < 0) {
        close(fd);
    }
}

void MiniExchangeGateway::modifyEpoll_(int fd, std::uint32_t events) {
    epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(epollFD_, EPOLL_CTL_MOD, fd, &ev) < 0) {
        handleError_(fd);
    }
}

void MiniExchangeGateway::removeFromEpoll_(int fd) {
    // Note: In kernel 2.6.9+, the event pointer can be NULL for EPOLL_CTL_DEL
    // But being explicit doesn't hurt
    epoll_event ev;
    epoll_ctl(epollFD_, EPOLL_CTL_DEL, fd, &ev);
}

void MiniExchangeGateway::setNonBlocking_(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        throw std::runtime_error("fcntl F_GETFL failed");
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        throw std::runtime_error("fcntl F_SETFL O_NONBLOCK failed");
    }
}

void MiniExchangeGateway::setTCPNoDelay_(int fd) {
    int flag = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
        std::cout << "[DEBUG] TCP_NODELAY could not be set\n";
    }
}

void MiniExchangeGateway::setupListenSocket_() {
    listenFD_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFD_ < 0) {
        throw std::runtime_error("Failed to create listen socket");
    }

    int reuse = 1;
    setsockopt(listenFD_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(listenFD_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(listenFD_);
        throw std::runtime_error("Failed to bind listen socket");
    }

    if (listen(listenFD_, SOMAXCONN) < 0) {
        close(listenFD_);
        throw std::runtime_error("Failed to listen on socket");
    }

    setNonBlocking_(listenFD_);
}

void MiniExchangeGateway::setupEpoll_() {
    epollFD_ = epoll_create1(0);
    if (epollFD_ < 0) {
        throw std::runtime_error("Failed to create epoll fd");
    }

    addToEpoll_(listenFD_, EPOLLIN | EPOLLET);
}
