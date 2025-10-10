#pragma once

#include <arpa/inet.h>
#include <chrono>
#include <iostream>
#include <poll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "client/client.hpp"
#include "utils/randomGenerator.hpp"

struct ClientState {
    ClientState(HMACKey h, ApiKey a) : c(h, a) {}
    int fd{-1};
    Client c;
    std::chrono::steady_clock::time_point nextOrder{std::chrono::steady_clock::now()};
    bool connected{false};
};

class ClientRunner {
public:
    ClientRunner(const std::string& serverIP, uint16_t port, size_t nClients,
                 uint64_t seed = 0)
        : serverIP_(serverIP), port_(port), rand_(seed) {
        clients_.reserve(nClients);
        for (size_t i{}; i < nClients; ++i) {
            ApiKey apiKey;
            HMACKey hmacKey;
            hmacKey.fill(0x11);
            apiKey.fill(0x22);
            clients_.emplace_back(hmacKey, apiKey);

            auto& session = clients_.back().c.getSession();

            session.lastHeartBeat = std::chrono::steady_clock::now() - rand_.jitter(2000);
        }
        std::cout << "Connecting " << nClients << " clients" << std::endl;
    }

    void start() {
        running_ = true;
        connectClients();
        setupPollfds();
        loginAll();

        std::cout << "Connected and logged in... sending orders" << std::endl;
        runLoop();
    }

    void stop() { running_ = false; }

    void loginAll() {
        for (auto& state : clients_) {
            if (state.connected) {
                state.c.sendHello();
                flushSendBuffer(state);
            }
        }
    }

private:
    void connectClients() {
        for (auto& state : clients_) {
            state.fd = socket(AF_INET, SOCK_STREAM, 0);
            if (state.fd < 0) {
                perror("socket");
                continue;
            }

            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port_);
            inet_pton(AF_INET, serverIP_.c_str(), &addr.sin_addr);

            if (connect(state.fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                perror("connect");
                close(state.fd);
                state.fd = -1;
            } else {
                state.connected = true;
            }
        }
    }

    void setupPollfds() {
        pollfds_.clear();
        for (auto& state : clients_) {
            if (state.connected) {
                pollfd pfd{};
                pfd.fd = state.fd;
                pfd.events = POLLIN | POLLOUT;
                pollfds_.push_back(pfd);
            }
        }
    }

    void runLoop() {
        using namespace std::chrono;
        constexpr milliseconds HEARTBEAT_INTERVAL{2000};

        while (running_) {
            int timeoutMs = 10; // poll timeout
            int nReady = poll(pollfds_.data(), pollfds_.size(), timeoutMs);
            auto now = steady_clock::now();

            if (nReady < 0) {
                perror("poll");
                break;
            }

            for (size_t i = 0; i < pollfds_.size(); ++i) {
                auto& pfd = pollfds_[i];
                auto& state = clients_[i];

                if (!state.connected) continue;

                if (pfd.revents & POLLIN) {
                    readIncoming(state);
                }

                if (pfd.revents & POLLOUT) {
                    flushSendBuffer(state);
                }

                auto& session = state.c.getSession();
                if (session.serverClientID != 0 &&
                    now - session.lastHeartBeat >= HEARTBEAT_INTERVAL) {
                    state.c.sendHeartbeat();
                    session.updateHeartbeat();
                }

                if (now >= state.nextOrder) {
                    generateAndSendOrder(state);
                    state.nextOrder = now + milliseconds(200) + rand_.jitter(100);
                }
            }
        }

        for (auto& state : clients_) {
            if (state.connected) close(state.fd);
        }
    }

    void generateAndSendOrder(ClientState& state) {
        bool isBuy = rand_.randomQty(0, 1);
        bool isLimit = rand_.randomQty(0, 1);
        Qty qty = rand_.randomQty(1, 100);
        Price price = rand_.randomPrice(990, 1010);
        state.c.sendOrder(qty, price, isBuy, isLimit);
    }

    void flushSendBuffer(ClientState& state) {
        auto& buf = state.c.getSession().sendBuffer;
        if (buf.empty()) return;

        ssize_t n = send(state.fd, buf.data(), buf.size(), 0);
        if (n > 0)
            buf.erase(buf.begin(), buf.begin() + n);
        else if (n < 0) {
            perror("send");
            state.connected = false;
            close(state.fd);
        }
    }

    void readIncoming(ClientState& state) {
        uint8_t tempBuf[4096];
        ssize_t n = recv(state.fd, tempBuf, sizeof(tempBuf), 0);
        if (n > 0) {
            state.c.appendRecvBuffer({tempBuf, static_cast<size_t>(n)});
            state.c.processIncoming();
        } else if (n == 0) {
            std::cerr << "Server closed connection\n";
            state.connected = false;
            close(state.fd);
        } else if (n < 0) {
            perror("recv");
            state.connected = false;
            close(state.fd);
        }
    }

    bool running_{false};
    std::string serverIP_;
    uint16_t port_;
    std::vector<pollfd> pollfds_;
    RandomGenerator rand_;

    std::vector<ClientState> clients_;
};