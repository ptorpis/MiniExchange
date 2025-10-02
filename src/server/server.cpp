#include "server/server.hpp"
#include "utils/utils.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

bool Server::start(uint16_t port) {
    epollFd_ = epoll_create1(0);
    if (epollFd_ < 0) {
        perror("epoll_create1");
        return false;
    }

    listenFd_ = createListenSocket(port);
    if (listenFd_ < 0) return false;

    epoll_event ev{};
    ev.data.fd = listenFd_;
    ev.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollFd_, EPOLL_CTL_ADD, listenFd_, &ev);

    running_ = true;
    return true;
}

void Server::stop() {
    running_ = false;

    if (listenFd_ >= 0) close(listenFd_);
    if (epollFd_ >= 0) close(epollFd_);

    // Iterate over all sessions
    for (auto& [fd, session] : sessionManager_.sessions()) {
        handleDisconnect(fd);
    }
}

void Server::acceptConnections() {
    sockaddr_in clientAddr{};
    socklen_t addrlen = sizeof(clientAddr);

    while (true) {
        int clientFD =
            accept4(listenFd_, (sockaddr*)&clientAddr, &addrlen, SOCK_NONBLOCK);
        if (clientFD < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            if (errno == EINTR)
                continue;
            else {
                perror("accept4");
                break;
            }
            return;
        }

        std::string ip = inet_ntoa(clientAddr.sin_addr);
        uint16_t port = ntohs(clientAddr.sin_port);

        connManager_.addConnection(port, ip, clientFD);

        epoll_event ev{};
        ev.data.fd = clientFD;
        ev.events = EPOLLIN | EPOLLET;
        epoll_ctl(epollFd_, EPOLL_CTL_ADD, clientFD, &ev);

        std::cout << "New Connection" << ip << ":" << port << " (fd=" << clientFD << ")"
                  << std::endl;
        handleRead(clientFD);
    }
}

void Server::handleRead(int fd) {
    Connection* conn = connManager_.getConnection(fd);
    if (!conn) return;
    conn->touch();

    Session* sess = sessionManager_.getSession(fd);
    if (!sess) {
        return;
    }

    while (true) {
        size_t oldSize = sess->recvBuffer.size();
        size_t freesSpace = sess->recvBuffer.capacity() - oldSize;

        if (freesSpace < 4096) {
            sess->recvBuffer.reserve(sess->recvBuffer.capacity() + 4096);
            freesSpace = sess->recvBuffer.capacity() - oldSize;
        }

        sess->recvBuffer.resize(oldSize + freesSpace);
        ssize_t n = ::read(fd, sess->recvBuffer.data() + oldSize, freesSpace);
        if (n > 0) {
            sess->recvBuffer.resize(oldSize + n);

            handler_.onMessage(fd);

            for (int outFd : handler_.getOutboundFDs()) {
                Session* outSess = sessionManager_.getSession(outFd);
                if (!outSess) continue;

                if (!outSess->sendBuffer.empty()) {
                    scheduleWrite(outFd);
                }
            }

        } else if (n == 0) {
            handleDisconnect(fd);
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                sess->recvBuffer.resize(oldSize);
                break;
            } else {
                handleDisconnect(fd);
                break;
            }
        }
    }
}

void Server::scheduleWrite(int fd) {
    epoll_event ev{};
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
    if (epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
        perror("epoll_ctl MOD scheduleWrite");
    }
}

void Server::handleWrite(int fd) {
    Session* sess = sessionManager_.getSession(fd);
    if (!sess) {
        return;
    }

    while (!sess->sendBuffer.empty()) {
        ssize_t n = ::write(fd, sess->sendBuffer.data(), sess->sendBuffer.size());
        if (n > 0) {
            sess->sendBuffer.erase(sess->sendBuffer.begin(),
                                   sess->sendBuffer.begin() + n);
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                handleDisconnect(fd);
                return;
            }
        }
    }

    if (sess->sendBuffer.empty()) {
        epoll_event ev{};
        ev.data.fd = fd;
        ev.events = EPOLLIN | EPOLLET;
        epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
    }
}

void Server::run() {
    std::vector<epoll_event> events(MAX_EVENTS);

    running_ = true;
    auto lastHeartbeatCheck = std::chrono::steady_clock::now();

    std::cout << "server running" << std::endl;
    while (running_) {
        int n = epoll_wait(epollFd_, events.data(), MAX_EVENTS, 1000);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i{}; i < n; ++i) {
            int fd = events[i].data.fd;

            if (fd == listenFd_) {
                acceptConnections();
            } else {
                if (events[i].events & EPOLLIN) {
                    handleRead(fd);
                }
                if (events[i].events & EPOLLOUT) {
                    handleWrite(fd);
                }
                if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                    handleDisconnect(fd);
                }
            }
        }

        auto now = std::chrono::steady_clock::now();
        if (now - lastHeartbeatCheck >= std::chrono::seconds(1)) {
            checkHeartbeats_();
            lastHeartbeatCheck = now;
        }
    }
}

void Server::handleDisconnect(int fd) {
    epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
    Connection* conn = connManager_.getConnection(fd);
    if (!conn) return;

    connManager_.removeConnection(fd);

    std::cout << "Disconnected fd=" << fd << std::endl;
}

int Server::createListenSocket(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }
    int flag = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    if (listen(fd, SOMAXCONN) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    return fd;
}

void Server::checkHeartbeats_() {
    auto now = std::chrono::steady_clock::now();
    auto& sessions = sessionManager_.sessions();
    /*

    for (size_t i{}; i < sessions.size();) {
        Session& sess = sessions[i];
        auto duration =
            std::chrono::duration_cast<std::chrono::seconds>(now - sess.lastHeartBeat)
                .count();

        if (duration > HEARTBEAT_TIMEOUT_SECONDS) {
            std::cout << "Client TIMEOUT fd=" << sess.FD << std::endl;
            handleDisconnect(sess.FD);
        } else {
            ++i;
        }
    }

    */
}