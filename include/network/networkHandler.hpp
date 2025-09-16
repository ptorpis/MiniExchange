#pragma once

#include "api/api.hpp"
#include "auth/sessionManager.hpp"
#include "protocol/messages.hpp"

#include <cstring>
#include <functional>
#include <optional>
#include <span>

class NetworkHandler {
public:
    using SendFn = std::function<void(Session&, const std::vector<uint8_t>&)>;

    /*
    Constructs the NetworkHandler with the given API and SessionManager

    The optional sendFn allows customizing how outgoing messages are being handled.

    The defauly funciton for this copes the outgoing message into the sendbuffer of
    of the given session
    */

    NetworkHandler(
        MiniExchangeAPI& api, SessionManager& sm,
        SendFn sendFn =
            [](Session& session, std::span<const uint8_t> buffer) {
                session.sendBuffer.insert(std::end(session.sendBuffer),
                                          std::begin(buffer), std::end(buffer));
            })
        : api_(api), sessionManager_(sm), sendFn_(std::move(sendFn)) {}

    void onMessage(int fd);
    void onDisconnect(int fd);

private:
    MiniExchangeAPI& api_;
    SessionManager& sessionManager_;
    SendFn sendFn_;

    std::optional<MessageHeader> peekHeader_(Session& Session) const;
    bool verifyHMAC_(const std::array<uint8_t, 32>& key, const uint8_t* data,
                     size_t dataLen, const uint8_t* expectedHMAC, size_t HMACLen);
    std::vector<uint8_t> computeHMAC_(const std::array<uint8_t, 32>& key,
                                      const uint8_t* data, size_t dataLen);

    void sendRaw_(Session& session, std::span<const std::uint8_t> buffer);
};