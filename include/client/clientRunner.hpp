#pragma once

#include <arpa/inet.h>
#include <chrono>
#include <iostream>
#include <poll.h>
#include <queue>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "client/client.hpp"
#include "utils/randomGenerator.hpp"
#include "utils/stats.hpp"

struct ClientState {
    ClientState(ApiKey a) : c(a) {}
    int fd{-1};
    Client c;
    bool connected{false};
};

enum class EventType { TradingAction, Heartbeat };

struct ScheduledEvent {
    std::chrono::steady_clock::time_point time;
    size_t clientIndex;
    EventType type;

    bool operator>(const ScheduledEvent& other) const { return time > other.time; }
};

struct ActionWeights {
    double newOrder = 0.6;
    double cancel = 0.3;
    double modify = 0.1;

    void normalize() {
        double total = newOrder + cancel + modify;
        if (total > 0) {
            newOrder /= total;
            cancel /= total;
            modify /= total;
        }
    }
};

class ClientRunner {
public:
    ClientRunner(const std::string& serverIP, uint16_t port, size_t nClients,
                 uint64_t seed = 0, ActionWeights weights = ActionWeights{})
        : serverIP_(serverIP), port_(port), rand_(seed), actionWeights_(weights) {

        actionWeights_.normalize();

        clients_.reserve(nClients);

        for (size_t i = 0; i < nClients; ++i) {
            ApiKey apiKey;
            apiKey.fill(0x22);
            clients_.emplace_back(apiKey);

            auto& session = clients_.back().c.getSession();
            session.lastHeartBeat = std::chrono::steady_clock::now() - rand_.jitter(500);
        }

        std::cout << "Connecting " << nClients << " clients" << std::endl;
    }

    void start(int durationSeconds = 30) {
        running_ = true;

        connectClients();
        setupPollfds();
        loginAll();

        std::cout << "Connected and logged in... starting simulation\n" << std::endl;

        std::thread statsThread([this, durationSeconds] {
            for (int i = 0; i < durationSeconds; ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if (!running_) {
                    return;
                }
                displayStats(i + 1, durationSeconds);
            }
        });

        std::thread timer([this, durationSeconds] {
            std::this_thread::sleep_for(std::chrono::seconds(durationSeconds));
            running_ = false;
        });

        runLoop();

        timer.join();
        statsThread.join();
        cleanup();

        stats_.printSummary();
    }

    void stop() { running_ = false; }

private:
    void displayStats(int currentSecond, int totalSeconds) {
        uint64_t actionsThisSecond = stats_.getActionsThisSecond();
        uint64_t totalActions = stats_.getTotalActions();

        std::cout << "\r[" << std::setw(2) << currentSecond << "/" << totalSeconds
                  << "s] " << "Actions/sec: " << std::setw(6) << actionsThisSecond
                  << " | " << "Total: " << std::setw(8) << totalActions << " "
                  << "(O:" << stats_.newOrdersThisSecond.load()
                  << " C:" << stats_.cancelsThisSecond.load()
                  << " M:" << stats_.modifiesThisSecond.load() << ")" << std::flush;

        stats_.resetPerSecondCounters();
    }

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

    void loginAll() {
        for (auto& state : clients_) {
            if (state.connected) {
                state.c.sendHello();
                flushSendBuffer(state);
            }
        }
    }

    void cleanup() {
        for (auto& state : clients_) {
            if (state.connected) {
                close(state.fd);
                state.connected = false;
            }
        }
    }

    void scheduleInitialEvents() {
        using namespace std::chrono;
        constexpr milliseconds HEARTBEAT_INTERVAL{2000};

        auto now = steady_clock::now();

        for (size_t i = 0; i < clients_.size(); ++i) {
            if (!clients_[i].connected) continue;

            eventQueue_.push({now + milliseconds(1000), i, EventType::TradingAction});
            // eventQueue_.push(
            //     {now + HEARTBEAT_INTERVAL + rand_.jitter(10), i,
            //     EventType::Heartbeat});
        }
    }

    void runLoop() {
        using namespace std::chrono;

        scheduleInitialEvents();

        while (running_) {
            auto now = steady_clock::now();

            int timeoutMs = calculatePollTimeout(now);
            int nReady = poll(pollfds_.data(), pollfds_.size(), timeoutMs);

            now = steady_clock::now();

            if (nReady > 0) {
                handlePollEvents();
            }

            processScheduledEvents(now);
        }
    }

    int calculatePollTimeout(std::chrono::steady_clock::time_point now) {
        if (eventQueue_.empty()) {
            return -1;
        }

        auto nextTime = eventQueue_.top().time;
        if (nextTime <= now) {
            return 0;
        }

        return std::chrono::duration_cast<std::chrono::milliseconds>(nextTime - now)
            .count();
    }

    void handlePollEvents() {
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
        }
    }

    void processScheduledEvents(std::chrono::steady_clock::time_point now) {
        while (!eventQueue_.empty() && eventQueue_.top().time <= now) {
            ScheduledEvent ev = eventQueue_.top();
            eventQueue_.pop();

            auto& state = clients_[ev.clientIndex];
            if (!state.connected) continue;

            handleEvent(ev, state, now);
        }
    }

    void handleEvent(ScheduledEvent& ev, ClientState& state,
                     std::chrono::steady_clock::time_point now) {
        using namespace std::chrono;

        switch (ev.type) {
        case EventType::TradingAction: {
            double roll = rand_.randomQty(0, 100) / 100.0;

            if (roll < actionWeights_.newOrder) {
                sendNewOrder(state);
            } else if (roll < actionWeights_.newOrder + actionWeights_.cancel) {
                sendCancelOrder(state);
            } else {
                sendModifyOrder(state);
            }

            ev.time = now + milliseconds(1) + rand_.jitter(5);
            eventQueue_.push(ev);
            break;
        }

        case EventType::Heartbeat:
            sendHeartbeat(state);
            ev.time = now + milliseconds(2000);
            eventQueue_.push(ev);
            break;
        }
    }

    void sendNewOrder(ClientState& state) {
        bool isBuy = rand_.randomQty(0, 1);
        bool isLimit = rand_.randomQty(0, 1);
        Qty qty = rand_.randomQty(100, 1000);
        Price price = rand_.randomPrice(990, 1010);
        state.c.sendOrder(qty, price, isBuy, isLimit);

        stats_.newOrders++;
        stats_.newOrdersThisSecond++;
    }

    void sendCancelOrder(ClientState& state) {
        const auto& orders = state.c.getOutstandingOrders();
        if (orders.empty()) {
            return;
        }

        auto it = orders.begin();
        size_t idx = rand_.randomQty(0, orders.size() - 1);
        std::advance(it, idx);

        state.c.sendCancel(it->second.id);

        stats_.cancels++;
        stats_.cancelsThisSecond++;
    }

    void sendModifyOrder(ClientState& state) {
        const auto& orders = state.c.getOutstandingOrders();
        if (orders.empty()) {
            return;
        }

        auto it = orders.begin();
        size_t idx = rand_.randomQty(0, orders.size() - 1);
        std::advance(it, idx);

        const auto& order = it->second;

        Qty newQty = order.qty + rand_.randomQty(-20, 20);
        Price newPrice = order.price + rand_.randomPrice(-5, 5);

        state.c.sendModify(order.id, newQty, newPrice);

        stats_.modifies++;
        stats_.modifiesThisSecond++;
    }

    void sendHeartbeat(ClientState& state) {
        auto& session = state.c.getSession();
        if (session.serverClientID != 0) {
            state.c.sendHeartbeat();
            session.updateHeartbeat();
            stats_.heartbeats++;
        }
    }

    void flushSendBuffer(ClientState& state) {
        auto& buf = state.c.getSession().sendBuffer;
        if (buf.empty()) return;

        ssize_t n = send(state.fd, buf.data(), buf.size(), 0);
        if (n > 0) {
            buf.erase(buf.begin(), buf.begin() + n);
        } else if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            perror("send");
            disconnectClient(state);
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
            disconnectClient(state);
        } else if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            perror("recv");
            disconnectClient(state);
        }
    }

    void disconnectClient(ClientState& state) {
        if (state.connected) {
            close(state.fd);
            state.connected = false;
            state.fd = -1;
        }
    }

    std::atomic<bool> running_{false};
    std::string serverIP_;
    uint16_t port_;
    std::vector<pollfd> pollfds_;
    RandomGenerator rand_;
    ActionWeights actionWeights_;
    utils::ClientStats stats_;
    std::vector<ClientState> clients_;
    std::priority_queue<ScheduledEvent, std::vector<ScheduledEvent>, std::greater<>>
        eventQueue_;
};