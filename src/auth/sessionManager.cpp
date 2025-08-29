#include "auth/sessionManager.hpp"
#include <chrono>

Session& SessionManager::createSession(int fd) {
    Session session(fd);
    session.serverClientID = generateClientToken();
    auto [it, inserted] = sessions_.emplace(fd, std::move(session));
    return it->second;
}

Session* SessionManager::getSession(int fd) {
    auto it = sessions_.find(fd);
    if (it != sessions_.end()) {
        return &it->second;
    }

    return nullptr;
}

void SessionManager::removeSession(int fd) {
    sessions_.erase(fd);
}