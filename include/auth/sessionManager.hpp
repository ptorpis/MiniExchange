#pragma once

#include "auth/session.hpp"
#include "logger/logger.hpp"
#include "utils/types.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <unordered_map>
#include <vector>

class SessionManager {
public:
    SessionManager(auto logger = nullptr) : logger_(logger) {}

    struct Heartbeat {
        int fd;
        std::chrono::steady_clock::time_point lastHeartbeat;
    };

    Session& createSession(int fd) {
        ClientID clientID = generateClientToken_();

        auto [it, _] = sessions_.emplace(fd, Session(fd));
        Session& sess = it->second;
        sess.serverClientID = clientID;
        sess.reserveBuffer();
        sess.clearBuffers();

        // temp, later remove and add database lookup after hello message
        std::fill(sess.hmacKey.begin(), sess.hmacKey.end(), 0x11);

        heartbeats_.push_back({fd, std::chrono::steady_clock::now()});
        fdToHbIndex_[fd] = heartbeats_.size() - 1;

        clientIDToFD_[clientID] = fd;

        return sess;
    }

    Session* getSession(int fd) {
        auto it = sessions_.find(fd);
        if (it != sessions_.end()) return &it->second;
        return nullptr;
    }

    Session* getSessionFromClientID(ClientID clientID) {
        auto it = clientIDToFD_.find(clientID);
        if (it == clientIDToFD_.end()) return nullptr;

        int fd = it->second;
        return getSession(fd);
    }

    void removeSession(int fd) {
        auto sessIt = sessions_.find(fd);
        if (sessIt == sessions_.end()) return;

        ClientID clientID = sessIt->second.serverClientID;

        sessions_.erase(sessIt);
        clientIDToFD_.erase(clientID);

        auto hbIt = fdToHbIndex_.find(fd);
        if (hbIt != fdToHbIndex_.end()) {
            size_t idx = hbIt->second;
            size_t lastIdx = heartbeats_.size() - 1;
            if (idx != lastIdx) {
                std::swap(heartbeats_[idx], heartbeats_[lastIdx]);
                fdToHbIndex_[heartbeats_[idx].fd] = idx;
            }
            heartbeats_.pop_back();
            fdToHbIndex_.erase(hbIt);
        }
    }

    void updateHb(int fd) {
        Session* sess = getSession(fd);
        if (!sess) return;

        auto idIt = fdToHbIndex_.find(fd);
        heartbeats_.at(idIt->second).lastHeartbeat = std::chrono::steady_clock::now();
    }

    [[nodiscard]] const std::vector<int>& getInactiveFDs(std::chrono::seconds timeout) {
        inactiveFDs_.clear();

        auto now = std::chrono::steady_clock::now();
        for (const auto& hb : heartbeats_) {
            auto diff =
                std::chrono::duration_cast<std::chrono::seconds>(now - hb.lastHeartbeat);

            if (diff > timeout) {
                inactiveFDs_.push_back(hb.fd);
            }
        }

        return inactiveFDs_;
    }

    std::unordered_map<int, Session>& getSessions() { return sessions_; }

private:
    ClientID generateClientToken_() { return ++clientToken_; }

    ClientID clientToken_{0};
    std::unordered_map<int, Session> sessions_;      // FD -> Session
    std::unordered_map<ClientID, int> clientIDToFD_; // ClientID -> FD

    std::vector<Heartbeat> heartbeats_;           // all heartbeats
    std::unordered_map<int, size_t> fdToHbIndex_; // FD -> heartbeat index
    std::vector<int> inactiveFDs_;

    std::shared_ptr<Logger<>> logger_;
};