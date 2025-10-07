#pragma once

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

#include "client/client.hpp"

class ClientNetwork {
public:
    ClientNetwork(const std::string& host, uint16_t port, Client& client)
        : host_(host), port_(port), client_(client) {}

    ~ClientNetwork() { disconnectServer(); }

    bool connectServer(int timeoutMs = 2000) {
        sock_.fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (sock_.fd < 0) {
            perror("socket");
            return false;
        }

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port_);
        if (::inet_pton(AF_INET, host_.c_str(), &serverAddr.sin_addr) <= 0) {
            perror("inet_pton");
            return false;
        }

        int res = ::connect(sock_.fd, (sockaddr*)&serverAddr, sizeof(serverAddr));
        if (res < 0 && errno != EINPROGRESS) {
            perror("connect");
            return false;
        }

        // Wait until socket is writable (connected or failed)
        struct pollfd pfd {
            sock_.fd, POLLOUT, 0
        };
        int pollRes = ::poll(&pfd, 1, timeoutMs);
        if (pollRes <= 0) {
            std::cerr << "connect timeout or error\n";
            return false;
        }

        int err = 0;
        socklen_t len = sizeof(err);
        getsockopt(sock_.fd, SOL_SOCKET, SO_ERROR, &err, &len);
        if (err != 0) {
            errno = err;
            perror("connect failed");
            return false;
        }

        std::cout << "Connected to " << host_ << ":" << port_ << "\n";
        return true;
    }

    void disconnectServer() {
        if (sock_.fd >= 0) {
            ::close(sock_.fd);
            sock_.fd = -1;
        }
    }

    bool sendMessage() {
        ClientSession& session = client_.getSession();
        size_t total = 0;

        while (total < session.sendBuffer.size()) {
            ssize_t n = ::send(sock_.fd, session.sendBuffer.data() + total,
                               session.sendBuffer.size() - total, 0);
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    waitWritable();
                    continue;
                }
                perror("send");
                throw std::runtime_error("send failed");
            }
            total += n;
        }

        session.sendBuffer.clear();
        return true;
    }

    bool receiveMessage() {
        auto& sess = client_.getSession();
        bool gotData = false;

        uint8_t tempBuf[4096];

        while (true) {
            ssize_t n = ::recv(sock_.fd, tempBuf, sizeof(tempBuf), 0);
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                } else {
                    perror("recv");
                    throw std::runtime_error("recv failed");
                }
            } else if (n == 0) {
                std::cerr << "Server closed connection\n";
                throw std::runtime_error("Server closed connection");
            } else {
                gotData = true;
                sess.recvBuffer.insert(sess.recvBuffer.end(), tempBuf, tempBuf + n);
            }
        }

        return gotData;
    }

    // Poll until readable with timeout
    bool waitReadable(int timeoutMs = 100) {
        struct pollfd pfd {
            sock_.fd, POLLIN, 0
        };
        int ret = ::poll(&pfd, 1, timeoutMs);
        return ret > 0 && (pfd.revents & POLLIN);
    }

    // Poll until writable (for send retries or connect)
    bool waitWritable(int timeoutMs = 100) {
        struct pollfd pfd {
            sock_.fd, POLLOUT, 0
        };
        int ret = ::poll(&pfd, 1, timeoutMs);
        return ret > 0 && (pfd.revents & POLLOUT);
    }

    int getSockFD() const { return sock_.fd; }

private:
    struct SocketHandle {
        int fd{-1};
        ~SocketHandle() {
            if (fd >= 0) ::close(fd);
        }
    };

    std::string host_;
    uint16_t port_;
    SocketHandle sock_;
    Client& client_;
};
