#pragma once

#include <unordered_map>

#include "auth/session.hpp"
#include "utils/types.hpp"

class SessionManager {
public:
    Session& createSession(int fd) {
        sessions_.emplace_back(fd);
        Session& sess = sessions_.back();
        sess.serverClientID = generateClientToken_();
        sess.reserveBuffer();
        sess.clearBuffers();
        std::fill(sess.hmacKey.begin(), sess.hmacKey.end(), 0x11);

        fdToIndex_[fd] = sessions_.size() - 1;
        return sess;
    }

    Session* getSession(int fd) {
        auto it = fdToIndex_.find(fd);
        if (it != fdToIndex_.end()) {
            return &sessions_[it->second];
        }

        return nullptr;
    }

    void removeSession(int fd) {
        auto it = fdToIndex_.find(fd);
        if (it == fdToIndex_.end()) return;

        size_t index = it->second;
        size_t lastIndex = sessions_.size() - 1;

        if (index != lastIndex) {
            std::swap(sessions_[index], sessions_[lastIndex]);
            fdToIndex_[sessions_[index].FD] = index;
        }

        sessions_.pop_back();
        fdToIndex_.erase(fd);
    }

    std::vector<Session>& sessions() { return sessions_; }

private:
    ClientID clientToken_{0};
    std::unordered_map<int, size_t> fdToIndex_;
    std::vector<Session> sessions_;

    ClientID generateClientToken_() { return ++clientToken_; }
};