#pragma once

#include "protocol/messages.hpp"
#include "protocol/server/serverMessages.hpp"

namespace server {

template <typename Payload> inline MessageHeader makeHeader(Session& session) {
    MessageHeader header{};
    header.messageType = +(PayloadTraits<Payload>::type);
    header.protocolVersionFlag = +(constants::HeaderFlags::PROTOCOL_VERSION);
    header.payLoadLength = static_cast<uint16_t>(PayloadTraits<Payload>::size);
    header.clientMsgSqn = session.clientSqn;
    header.serverMsgSqn = ++session.serverSqn;
    std::memset(header.reservedFlags, 0, sizeof(header.reservedFlags));
    std::memset(header.padding, 0, sizeof(header.padding));

    return header;
}

struct MessageFactory {
    static Message<server::HelloAckPayload>
    makeHelloAck(Session& session, statusCodes::HelloStatus status) {
        Message<server::HelloAckPayload> msg;

        msg.header = makeHeader<server::HelloAckPayload>(session);

        msg.payload.serverClientID = session.serverClientID;
        msg.payload.status = +(status);

        std::fill(std::begin(msg.payload.hmac), std::end(msg.payload.hmac), 0x00);
        std::fill(std::begin(msg.payload.padding), std::end(msg.payload.padding), 0x00);

        return msg;
    }

    static Message<server::LogoutAckPayload>
    makeLoutAck(Session& session, statusCodes::LogoutStatus status) {
        Message<server::LogoutAckPayload> msg;

        msg.header = makeHeader<server::LogoutAckPayload>(session);

        msg.payload.serverClientID = session.serverClientID;
        msg.payload.status = +(status);

        std::fill(std::begin(msg.payload.hmac), std::end(msg.payload.hmac), 0x00);
        std::fill(std::begin(msg.payload.padding), std::end(msg.payload.padding), 0x00);

        return msg;
    }

    static Message<server::OrderAckPayload>
    makeOrderAck(Session& session, OrderRequest& req, std::optional<OrderID> orderID,
                 statusCodes::OrderAckStatus status, Timestamp ts) {

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

        std::fill(std::begin(ack.payload.hmac), std::end(ack.payload.hmac), 0x00);
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

        std::fill(std::begin(msg.payload.hmac), std::end(msg.payload.hmac), 0x00);

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
        std::fill(std::begin(msg.payload.hmac), std::end(msg.payload.hmac), 0x00);

        return msg;
    }

    static Message<server::ModifyAckPayload>
    makeModifyAck(Session& session, OrderID oldOrderID, OrderID newOrderID, Qty newQty,
                  Price newPrice, statusCodes::ModifyStatus status) {
        Message<server::ModifyAckPayload> msg;
        msg.header = makeHeader<server::ModifyAckPayload>(session);
        msg.payload.oldServerOrderID = oldOrderID;
        msg.payload.newServerOrderID = newOrderID;
        msg.payload.newQty = newQty;
        msg.payload.newPrice = newPrice;
        msg.payload.status = +(status);

        std::fill(std::begin(msg.payload.padding), std::end(msg.payload.padding), 0x00);
        std::fill(std::begin(msg.payload.hmac), std::end(msg.payload.hmac), 0x00);

        return msg;
    }
};
} // namespace server
