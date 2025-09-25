#pragma once

#include "api/api.hpp"
#include "auth/sessionManager.hpp"
#include "protocol/messages.hpp"

#include <cstring>
#include <functional>
#include <optional>
#include <span>

class ProtocolHandler {
public:
    using SendFn = std::function<void(Session&, const std::span<const uint8_t>)>;

    /*
    Constructs the NetworkHandler with the given API and SessionManager

    The optional sendFn allows customizing how outgoing messages are being handled.

    The defauly funciton for this copies the outgoing message into the sendbuffer of
    of the given session
    */

    ProtocolHandler(SendFn sendFn =
                        [](Session& session, const std::span<const uint8_t> buffer) {
                            session.sendBuffer.insert(std::end(session.sendBuffer),
                                                      std::begin(buffer),
                                                      std::end(buffer));
                        })
        : api_(MiniExchangeAPI()), sendFn_(std::move(sendFn)) {}

    void onMessage(int fd);
    void onDisconnect(int fd);

    Session& createSession(int fd) { return api_.connectClient(fd); }
    Session* getSession(int fd) { return api_.getSession(fd); }
    void disconnectSession(int fd) { return api_.disconnectClient(fd); }

    MiniExchangeAPI* getAPI() { return &api_; }

private:
    MiniExchangeAPI api_;
    SendFn sendFn_; // custom send function, assigned at construction

    std::optional<MessageHeader> peekHeader_(Session& Session) const;
    bool verifyHMAC_(const std::array<uint8_t, 32>& key, const uint8_t* data,
                     size_t dataLen, const uint8_t* expectedHMAC, size_t HMACLen);
    std::vector<uint8_t> computeHMAC_(const std::array<uint8_t, 32>& key,
                                      const uint8_t* data, size_t dataLen);
};