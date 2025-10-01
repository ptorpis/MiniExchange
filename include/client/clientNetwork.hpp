#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>
#include <vector>

#include "client/client.hpp"

class ClientNetwork {
public:
    ClientNetwork(const std::string& host, uint16_t port, Client& client)
        : host_(host), port_(port), client_(client) {}

    bool connectServer() {
        sockFD_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (sockFD_ < 0) {
            perror("socket");
            return false;
        }

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port_);
        inet_pton(AF_INET, host_.c_str(), &serverAddr.sin_addr);

        int res = ::connect(sockFD_, (sockaddr*)&serverAddr, sizeof(serverAddr));
        if (res < 0 && errno != EINPROGRESS) {
            perror("connect");
            return false;
        }

        return true;
    }

    void disconnectServer() {
        if (sockFD_ >= 0) {
            close(sockFD_);
            sockFD_ = -1;
        }
    }

    void sendMessage() {
        ClientSession& session = client_.getSession();
        size_t total{};
        while (total < session.sendBuffer.size()) {
            ssize_t n = ::send(sockFD_, session.sendBuffer.data() + total,
                               session.sendBuffer.size() - total, 0);

            if (n < 0) throw std::runtime_error("send failed");
            total += n;
        }

        session.sendBuffer.clear();
    }

    void receiveMessage() {
        auto& sess = client_.getSession();

        constexpr size_t BUFFER_CHUNK = 4096;

        while (true) {
            size_t oldSize = sess.recvBuffer.size();
            sess.recvBuffer.resize(oldSize + BUFFER_CHUNK);

            ssize_t n =
                ::recv(sockFD_, sess.recvBuffer.data() + oldSize, BUFFER_CHUNK, 0);
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    sess.recvBuffer.resize(oldSize);
                    break;
                } else {
                    perror("recv");
                    disconnectServer();
                    return;
                }
            } else if (n == 0) {
                std::cout << "Server closed connection\n";
                sess.recvBuffer.resize(oldSize);
                disconnectServer();
                return;
            }

            sess.recvBuffer.resize(oldSize + n);
        }
    }

    int getSockFD() const { return sockFD_; }

private:
    std::string host_;
    uint16_t port_;
    int sockFD_{-1};
    int epollFD_{-1};
    Client& client_;
};