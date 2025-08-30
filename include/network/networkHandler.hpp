#pragma once

#include "api/api.hpp"
#include "auth/sessionManager.hpp"
#include "protocol/messages.hpp"

#include <cstring>
#include <optional>
#include <span>

class NetworkHandler {
public:
    NetworkHandler(MiniExchangeAPI& api, SessionManager& sm)
        : api_(api), sessionManager_(sm) {}

    void onMessage(int fd);
    void onDisconnect(int fd);

private:
    MiniExchangeAPI& api_;
    SessionManager& sessionManager_;

    std::optional<MessageHeader> peekHeader_(Session& Session) const;
    bool isMessageComplete_(Session& session, const uint16_t payloadLength);

    void sendRaw_(Session& session, std::span<const std::uint8_t> buffer);
};