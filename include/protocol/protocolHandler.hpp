#pragma once

#include "api/api.hpp"
#include "protocol/messages.hpp"
#include "protocol/serverMessages.hpp"
#include "sessions/sessionManager.hpp"
#include "utils/status.hpp"
#include "utils/types.hpp"
#include <optional>
#include <unordered_set>

class ProtocolHandler {
public:
    ProtocolHandler(SessionManager& sm, MiniExchangeAPI& api)
        : sessionManager_(sm), api_(api), dirtyFDs_() {}

    void onMessage(int fd);
    [[nodiscard]] std::unordered_set<int>& getDirtyFDs() { return dirtyFDs_; }

    void clearDirtyFD(int fd) { dirtyFDs_.erase(fd); }

private:
    void processMessages_(Session& session);
    std::size_t handleMessage_(Session& session, std::span<const std::byte> messageBytes);

    std::size_t handleHello_(Session& session, std::span<const std::byte> msg);
    std::size_t handleLogout_(Session& session, std::span<const std::byte> mgs);
    std::size_t handleNewOrder_(Session& session, std::span<const std::byte> msg);
    std::size_t handleModifyOrder_(Session& session, std::span<const std::byte> msg);
    std::size_t handleCancel_(Session& session, std::span<const std::byte> msg);

    std::optional<MessageHeader> peekHeader_(std::span<const std::byte> view) const;

    Message<server::HelloAckPayload> makeHelloAck_(Session& session,
                                                   status::HelloAckStatus statusCode);
    Message<server::LogoutAckPayload> makeLogoutAck_(Session& session,
                                                     status::LogoutAckStatus statusCode);
    Message<server::OrderAckPayload> makeOrderAck_(Session& session,
                                                   const MatchResult& result);
    Message<server::TradePayload> makeTradeMsg_(Session& session, const TradeEvent& ev,
                                                bool isBuyer);
    Message<server::ModifyAckPayload> makeModifyAck_(Session& session,
                                                     const ModifyResult& res);
    Message<server::CancelAckPayload> makeCancelAck_(Session& session, OrderID orderID,
                                                     InstrumentID instrID, bool success);

    SessionManager& sessionManager_;
    MiniExchangeAPI& api_;

    std::unordered_set<int> dirtyFDs_;
};
