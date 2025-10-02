#pragma once

#include <memory>
#include <unordered_map>

#include "auth/session.hpp"
#include "logger/logger.hpp"
#include "utils/types.hpp"

class SessionManager {
public:
    SessionManager(std::shared_ptr<Logger> logger = nullptr) : logger_(logger) {}

    Session& createSession(int fd) {

        Session sess(fd);
        ClientID id = generateClientToken_();
        sess.serverClientID = id;
        sess.reserveBuffer();
        sess.clearBuffers();

        // temp, later remove and add database lookup after hello message
        std::fill(sess.hmacKey.begin(), sess.hmacKey.end(), 0x11);

        auto [it, _] = sessions_.emplace(fd, std::move(sess));
        idToFD_[id] = fd;

        heartbeats_.push_back({fd, std::chrono::steady_clock::now()});
        fdToHbId_[fd] = heartbeats_.size() - 1;

        return it->second;
    }

    Session* getSession(int fd) {
        auto it = sessions_.find(fd);
        if (it != sessions_.end()) {
            it->second;
        }

        return nullptr;
    }

    Session* getSessionFromID(ClientID clientID) {
        auto idIt = idToFD_.find(clientID);
        if (idIt == idToFD_.end()) {
            return nullptr;
        }

        auto sessionIt = sessions_.find(idIt->first);
        if (sessionIt != sessions_.end()) {
            return &sessionIt->second;
        }

        return nullptr;
    }

    void removeSession(int fd) {
        auto it = sessions_.find(fd);
        if (it == sessions_.end()) return;

        ClientID id = it->second.serverClientID;
        idToFD_.erase(id);
        sessions_.erase(it);

        auto hbIt = fdToHbId_.find(fd);
        if (hbIt != fdToHbId_.end()) {
            size_t idx = hbIt->second;
            size_t last = heartbeats_.size() - 1;

            if (idx != last) {
                std::swap(heartbeats_[idx], heartbeats_[last]);
                fdToHbId_[heartbeats_[idx].fd] = idx;
            }

            heartbeats_.pop_back();
            fdToHbId_.erase(hbIt);
        }
    }

    void updateHeartbeat(int fd) {
        auto it = fdToHbId_.find(fd);
        if (it != fdToHbId_.end()) {
            heartbeats_[it->second].lastHeartbeat = std::chrono::steady_clock::now();
        }
    }

    void checkHeartbeats(std::chrono::seconds timeout) {
        auto now = std::chrono::steady_clock::now();
        for (size_t i = 0; i < heartbeats_.size();) {
            if (now - heartbeats_[i].lastHeartbeat > timeout) {
                removeSession(heartbeats_[i].fd);
            } else {
                ++i;
            }
        }
    }

    void disconnectAll() {
        for (auto session : sessions_) {
            const int fd = session.first;
            removeSession(fd);
        }
    }

private:
    struct Heartbeat {
        int fd;
        std::chrono::steady_clock::time_point lastHeartbeat;
    };

    ClientID clientToken_{0};
    std::unordered_map<int, Session> sessions_;
    std::unordered_map<ClientID, int> idToFD_;
    std::unordered_map<int, size_t> fdToHbId_; // fd -> heartbeats_ index;

    std::vector<Heartbeat> heartbeats_;

    std::shared_ptr<Logger> logger_;

    ClientID generateClientToken_() { return ++clientToken_; }
};