#pragma once

#include "api/api.hpp"
#include "protocol/messages.hpp"
#include "sessions/sessionManager.hpp"
#include <optional>

class ProtocolHandler {
public:
    ProtocolHandler(SessionManager& sm, MiniExchangeAPI& api)
        : sessionManager_(sm), api_(api) {}

    void onMessage(int fd);

private:
    void processMessages_(Session& session);
    std::size_t handleMessage_(Session& session, std::span<const std::byte> messageBytes);
    std::size_t handleHello_(Session& session, std::span<const std::byte> msg);
    std::size_t handleLogout_(Session& session, std::span<const std::byte> mgs);
    std::size_t handleNewOrder_(Session& session, std::span<const std::byte> msg);
    std::size_t handleModifyOrder_(Session& session, std::span<const std::byte> msg);
    std::size_t handleCancel_(Session& session, std::span<const std::byte> msg);

    std::optional<MessageHeader> peekHeader_(std::span<const std::byte> view) const;

    SessionManager& sessionManager_;
    MiniExchangeAPI& api_;
};
