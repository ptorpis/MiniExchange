#pragma once

#include "api/api.hpp"
#include "auth/sessionManager.hpp"
#include "protocol/messages.hpp"
#include "server/clients.hpp"
#include "utils/utils.hpp"

#include <cstring>
#include <functional>
#include <iostream>
#include <optional>
#include <span>

using OutBoundFDs = std::vector<int>;

class ProtocolHandler {
public:
    using SendFn = std::function<void(Session&, const std::span<const uint8_t>)>;

    /*
    Constructs the NetworkHandler with the given API and SessionManager

    The optional sendFn allows customizing how outgoing messages are being handled.

    The defauly funciton for this copies the outgoing message into the sendbuffer of
    of the given session
    */

    ProtocolHandler(
        SessionManager& sm, std::shared_ptr<Logger<>> logger = nullptr,
        SendFn sendFn =
            [](Session& session, const std::span<const uint8_t> buffer) {
                session.sendBuffer.insert(std::end(session.sendBuffer),
                                          std::begin(buffer), std::end(buffer));
            })
        : api_(MiniExchangeAPI(sm, logger)), sendFn_(std::move(sendFn)),
          clientManager_(ClientManager()), logger_(logger) {
        clientManager_.addTestDefault();
        outBoundFDs_.reserve(16);
        lastScreenUpdate_ = std::chrono::steady_clock::now();
    }

    void onMessage(int fd);

    Session& createSession(int fd) { return api_.connectClient(fd); }
    Session* getSession(int fd) { return api_.getSession(fd); }
    void disconnectSession(int fd) { return api_.disconnectClient(fd); }

    MiniExchangeAPI* getAPI() { return &api_; }

    OutBoundFDs& getOutboundFDs() { return outBoundFDs_; }

private:
    MiniExchangeAPI api_;
    SendFn sendFn_; // custom send function, assigned at construction
    ClientManager clientManager_;
    std::shared_ptr<Logger<>> logger_;

    OutBoundFDs outBoundFDs_;

    std::optional<MessageHeader> peekHeader_(Session& Session) const;
    bool verifyHMAC_(const std::array<uint8_t, 32>& key, const uint8_t* data,
                     size_t dataLen, const uint8_t* expectedHMAC, size_t HMACLen);
    std::vector<uint8_t> computeHMAC_(const std::array<uint8_t, 32>& key,
                                      const uint8_t* data, size_t dataLen);

    std::chrono::steady_clock::time_point lastScreenUpdate_;
};