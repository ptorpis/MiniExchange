#pragma once

#include "auth/sessionManager.hpp"
#include "core/matchingEngine.hpp"
#include "core/order.hpp"
#include "core/service.hpp"
#include "logger/logger.hpp"
#include "protocol/messages.hpp"
#include "protocol/statusCodes.hpp"
#include <optional>

class MiniExchangeAPI {
public:
    MiniExchangeAPI(SessionManager& sm, std::shared_ptr<Logger> logger = nullptr)
        : engine_(MatchingEngine(logger)), sessionManager_(sm) {}

    Session& connectClient(int fd);
    Session* getSession(int fd);
    Session* getSessionFromID(ClientID clientID);
    void disconnectClient(int fd);

    std::vector<uint8_t> handleHello(Session& session, Message<client::HelloPayload> msg);

    std::vector<uint8_t> handleLogout(Session& session,
                                      Message<client::LogoutPayload> msg);
    std::vector<OutboundMessage> handleNewOrder(Session& session,
                                                Message<client::NewOrderPayload>& msg);
    std::vector<uint8_t> handleCancel(Session& session,
                                      Message<client::CancelOrderPayload>& msg);
    std::vector<OutboundMessage> handleModify(Session& session,
                                              Message<client::ModifyOrderPayload>& msg);

    std::optional<Price> getSpread() const { return engine_.getSpread(); };
    std::optional<Price> getBestAsk() const { return engine_.getBestAsk(); };
    std::optional<Price> getBestBid() const { return engine_.getBestBid(); };

    std::optional<const Order*> getOrder(OrderID orderID) const {
        return engine_.getOrder(orderID);
    }

    size_t getAskSize() const { return engine_.getAskSize(); }
    size_t getBidsSize() const { return engine_.getBidsSize(); }

    size_t getAsksPriceLevelSize(Price price) {
        return engine_.getAsksPriceLevelSize(price);
    }
    size_t getBidsPriceLevelSize(Price price) {
        return engine_.getBidsPriceLevelSize(price);
    }

    auto getBidsSnapshot() const { return engine_.getBidsSnapshot(); }

    auto getAsksSnapshot() const { return engine_.getAsksSnapshot(); }

    void updateHb(int fd) { sessionManager_.updateHb(fd); }

private:
    MatchingEngine engine_;
    SessionManager& sessionManager_;

    bool isValidAPIKey_(Session& session, const uint8_t key[16]);
    std::vector<uint8_t> computeHMAC_(const std::array<uint8_t, 32>& key,
                                      const uint8_t* data, size_t dataLen);
    std::vector<uint8_t> makeHelloAck_(Session& session,
                                       statusCodes::HelloAckStatus status);
    std::vector<uint8_t> makeLogoutAck_(Session& session,
                                        statusCodes::LogoutAckStatus status);
    std::vector<uint8_t> makeTradeMsg_(Session& session, TradeEvent& trade, bool isBuyer);
    std::vector<uint8_t> makeOrderAck_(Session& session, OrderRequest& req,
                                       std::optional<OrderID> orderID,
                                       statusCodes::OrderAckStatus status);
    std::vector<uint8_t> makeCancelAck_(Session& session, const OrderID orderID,
                                        statusCodes::CancelAckStatus status);

    std::vector<uint8_t> makeModifyAck_(Session& session, OrderID oldOrderID,
                                        OrderID newOrderID, Qty newQty, Price newPrice,
                                        statusCodes::ModifyAckStatus status);
};