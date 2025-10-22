#pragma once

#include "api/api.hpp"
#include "auth/sessionManager.hpp"
#include "events/eventBus.hpp"
#include "protocol/messages.hpp"
#include "utils/stats.hpp"
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

    The default function for this copies the outgoing message into the sendbuffer of
    of the given session
    */

    ProtocolHandler(
        SessionManager& sm, std::shared_ptr<EventBus> evBus = nullptr,
        SendFn sendFn =
            [](Session& session, const std::span<const uint8_t> buffer) {
                session.sendBuffer.insert(std::end(session.sendBuffer),
                                          std::begin(buffer), std::end(buffer));
            })
        : api_(MiniExchangeAPI(sm, evBus)), sendFn_(std::move(sendFn)), evBus_(evBus) {
        outBoundFDs_.reserve(16);
        lastScreenUpdate_ = std::chrono::steady_clock::now();
    }

    void onMessage(int fd);

    Session& createSession(int fd) { return api_.connectClient(fd); }
    Session* getSession(int fd) { return api_.getSession(fd); }
    void disconnectSession(int fd) { return api_.disconnectClient(fd); }

    MiniExchangeAPI* getAPI() { return &api_; }

    OutBoundFDs& getOutboundFDs() { return outBoundFDs_; }

    auto getBidsSnapshot() const { return api_.getBidsSnapshot(); }

    auto getAsksSnapshot() const { return api_.getAsksSnapshot(); }

private:
    MiniExchangeAPI api_;
    SendFn sendFn_; // custom send function, assigned at construction
    std::shared_ptr<EventBus> evBus_;

    OutBoundFDs outBoundFDs_;

    std::optional<MessageHeader> peekHeader_(Session& Session) const;

    std::chrono::steady_clock::time_point lastScreenUpdate_;
    int timingSampleCounter_{0};
    int TIMING_SAMPLE_INTERVAL{10};
};