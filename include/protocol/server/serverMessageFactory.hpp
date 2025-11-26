#pragma once

#include "auth/session.hpp"
#include "core/order.hpp"
#include "events/events.hpp"
#include "protocol/messages.hpp"
#include "protocol/server/serverMessages.hpp"
#include "protocol/statusCodes.hpp"
#include "protocol/traits.hpp"
#include "utils/types.hpp"

namespace server {

template <typename Payload> inline MessageHeader makeHeader(Session& session) {
    MessageHeader header{};
    header.messageType = +(server::PayloadTraits<Payload>::type);
    header.protocolVersionFlag = +(constants::HeaderFlags::PROTOCOL_VERSION);
    header.payLoadLength = static_cast<uint16_t>(PayloadTraits<Payload>::size);
    header.clientMsgSqn = session.clientSqn;
    header.serverMsgSqn = ++session.serverSqn;
    std::memset(header.padding, 0, sizeof(header.padding));

    return header;
}

struct MessageFactory {
    static Message<server::HelloAckPayload>
    makeHelloAck(Session& session, statusCodes::HelloAckStatus status) {
        Message<server::HelloAckPayload> msg;

        msg.header = makeHeader<server::HelloAckPayload>(session);

        msg.payload.serverClientID = session.serverClientID;
        msg.payload.status = +(status);

        std::fill(std::begin(msg.payload.padding), std::end(msg.payload.padding), 0x00);

        return msg;
    }

    static Message<server::LogoutAckPayload>
    makeLoutAck(Session& session, statusCodes::LogoutAckStatus status) {
        Message<server::LogoutAckPayload> msg;

        msg.header = makeHeader<server::LogoutAckPayload>(session);

        msg.payload.serverClientID = session.serverClientID;
        msg.payload.status = +(status);

        std::fill(std::begin(msg.payload.padding), std::end(msg.payload.padding), 0x00);

        return msg;
    }

    static Message<server::OrderAckPayload>
    makeOrderAck(Session& session, OrderRequest& req, std::optional<OrderID> orderID,
                 statusCodes::OrderAckStatus status) {

        Message<server::OrderAckPayload> ack;
        Timestamp currentTime = utils::getCurrentTimestampMicros();
        ack.header = makeHeader<server::OrderAckPayload>(session);
        ack.payload.serverClientID = session.serverClientID;
        ack.payload.instrumentID = req.instrumentID;
        ack.payload.status = +(status);
        ack.payload.serverTime = currentTime;
        ack.payload.acceptedQty = (req.type == OrderType::LIMIT) ? req.qty : 0;
        if (!orderID) {
            ack.payload.serverOrderID = 0x00;
            ack.payload.acceptedPrice = 0;
        } else {
            ack.payload.serverOrderID = orderID.value();
            ack.payload.acceptedPrice = req.price;
        }

        std::fill(std::begin(ack.payload.padding), std::end(ack.payload.padding), 0x00);
        return ack;
    }

    static Message<server::TradePayload> makeTradeMsg(Session& session, TradeEvent& trade,
                                                      bool isBuyer) {
        Message<server::TradePayload> msg;
        msg.header = makeHeader<server::TradePayload>(session);

        msg.payload.tradeID = session.getNextExeID();
        msg.payload.filledQty = trade.qty;
        msg.payload.filledPrice = trade.price;
        msg.payload.timestamp = utils::getCurrentTimestampMicros();

        if (isBuyer) {
            msg.payload.serverClientID = trade.buyerID;
            msg.payload.serverOrderID = trade.buyerOrderID;
        } else {
            msg.payload.serverClientID = trade.sellerID;
            msg.payload.serverOrderID = trade.sellerOrderID;
        }

        return msg;
    }

    static Message<server::CancelAckPayload>
    makeCancelAck(Session& session, const OrderID orderID,
                  statusCodes::CancelAckStatus status) {
        Message<server::CancelAckPayload> msg;
        msg.header = makeHeader<server::CancelAckPayload>(session);
        msg.payload.serverClientID = session.serverClientID;
        msg.payload.serverOrderID = orderID;
        msg.payload.status = +(status);

        std::fill(std::begin(msg.payload.padding), std::end(msg.payload.padding), 0x00);

        return msg;
    }

    static Message<server::ModifyAckPayload>
    makeModifyAck(Session& session, OrderID oldOrderID, OrderID newOrderID, Qty newQty,
                  Price newPrice, statusCodes::ModifyAckStatus status) {
        Message<server::ModifyAckPayload> msg;
        msg.header = makeHeader<server::ModifyAckPayload>(session);
        msg.payload.oldServerOrderID = oldOrderID;
        msg.payload.newServerOrderID = newOrderID;
        msg.payload.newQty = newQty;
        msg.payload.newPrice = newPrice;
        msg.payload.status = +(status);

        std::fill(std::begin(msg.payload.padding), std::end(msg.payload.padding), 0x00);

        return msg;
    }
};
} // namespace server
