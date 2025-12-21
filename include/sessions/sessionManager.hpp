#pragma once

#include "sessions/session.hpp"
#include "utils/types.hpp"
#include <unordered_map>

class SessionManager {
public:
    SessionManager() : clientToken_(0) {}

    Session& createSession(int fd) {
        ClientID clientID = getNextClientID_();
        auto [it, _] = sessions_.emplace(fd, Session(fd, clientID));
        clientIDToFD_[clientID] = fd;

        Session& sess = it->second;

        return sess;
    }

    // get the session from file descriptor, one lookup
    Session* getSession(int fd) {
        auto it = sessions_.find(fd);
        if (it != sessions_.end()) return &it->second;
        return nullptr;
    }

    // get the session from the clientid, 2 lookups
    Session* getSession(ClientID clientID) {
        if (auto it = clientIDToFD_.find(clientID); it != clientIDToFD_.end()) {
            return getSession(it->second);
        }
        return nullptr;
    }

    void removeSession(int fd) {
        auto sessIt = sessions_.find(fd);
        if (sessIt == sessions_.end()) return;

        clientIDToFD_.erase(sessIt->second.getClientID());
        sessions_.erase(sessIt);
    }

    void authenticateClient(int fd) {
        if (auto it = sessions_.find(fd); it != sessions_.end()) {
            it->second.authenticate();
        }
    }

    std::unordered_map<int, Session>& getSessions() { return sessions_; }

private:
    ClientID clientToken_;

    ClientID getNextClientID_() { return ++clientToken_; }

    std::unordered_map<int, Session> sessions_;
    std::unordered_map<ClientID, int> clientIDToFD_;
};
