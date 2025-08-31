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
    bool verifyHMAC_(const std::array<uint8_t, 32>& key, const uint8_t* data,
                     size_t dataLen, const uint8_t* expectedHMAC, size_t HMACLen);
    std::vector<uint8_t> computeHMAC_(const std::array<uint8_t, 32>& key,
                                      const uint8_t* data, size_t dataLen);

    void sendRaw_(Session& session, std::span<const std::uint8_t> buffer);
};