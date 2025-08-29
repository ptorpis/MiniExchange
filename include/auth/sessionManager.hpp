#pragma once

#include <unordered_map>

#include "auth/session.hpp"
#include "utils/types.hpp"

class SessionManager {
public:
    Session& createSession(int fd);
    void removeSession(int fd);
    Session* getSession(int fd);

    ClientID generateClientToken() { return ++clientToken_; }

private:
    ClientID clientToken_{0};
    std::unordered_map<int, Session> sessions_;
};