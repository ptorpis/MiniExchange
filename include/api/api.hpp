#pragma once

#include "auth/sessionManager.hpp"
#include "core/matchingEngine.hpp"
#include "core/order.hpp"
#include "core/service.hpp"
#include "protocol/messages.hpp"
#include "protocol/statusCodes.hpp"
#include <optional>

class MiniExchangeAPI {
public:
    MiniExchangeAPI(MatchingEngine& engine, SessionManager& sm, OrderService& service)
        : engine_(engine), sessionManager_(sm), service_(service) {}

    Session& connectClient(int fd);
    Session* getSession(int fd);
    void disconnectClient(int fd);

    std::vector<uint8_t> handleHello(Session& session, Message<HelloPayload> msg);

    std::vector<uint8_t> handleLogout(Session& session, Message<LogoutPayload> msg);
    std::vector<OutboundMessage> handleNewOrder(Session& session,
                                                Message<NewOrderPayload>& msg);
    std::vector<uint8_t> handleCancel(Session& session, Message<CancelOrderPayload>& msg);
    std::vector<OutboundMessage> handleModify(Session& session,
                                              Message<ModifyOrderPayload>& msg);

private:
    MatchingEngine& engine_;
    SessionManager& sessionManager_;
    OrderService& service_;

    template <typename Payload>
    void sendMessage_(Session& sesssion, const Message<Payload>& msg);

    bool isValidAPIKey_(Session& session, const uint8_t key[16]);
    std::vector<uint8_t> computeHMAC_(const std::array<uint8_t, 32>& key,
                                      const uint8_t* data, size_t dataLen);
    std::vector<uint8_t> makeHelloAck_(Session& session, statusCodes::HelloStatus status);
    std::vector<uint8_t> makeLogoutAck_(Session& session,
                                        statusCodes::LogoutStatus status);
    std::vector<uint8_t> makeTradeMsg_(Session& session, TradeEvent& trade, bool isBuyer);
    std::vector<uint8_t> makeOrderAck_(Session& session, OrderRequest& req,
                                       std::optional<OrderID> orderID, Timestamp ts,
                                       statusCodes::OrderAckStatus status);
    std::vector<uint8_t> makeCancelAck_(Session& session, const OrderID orderID,
                                        statusCodes::CancelAckStatus status);

    std::vector<uint8_t> makeModifyAck_(Session& session, OrderID oldOrderID,
                                        OrderID newOrderID,
                                        statusCodes::ModifyStatus status);
};