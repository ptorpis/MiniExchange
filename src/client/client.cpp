#include "client/client.hpp"
#include "protocol/client/clientMessageFactory.hpp"
#include "protocol/client/clientMessages.hpp"
#include "protocol/server/serverMessages.hpp"
#include "protocol/traits.hpp"
#include "utils/utils.hpp"
#include <arpa/inet.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <utility>

void Client::eraseBytesFromBuffer(std::vector<uint8_t>& buffer, size_t n_bytes) {
    buffer.erase(buffer.begin(), buffer.begin() + n_bytes);
}

void Client::sendHello() {
    Message<client::HelloPayload> msg;
    msg.header = client::makeClientHeader<client::HelloPayload>(session_);
    std::memcpy(msg.payload.apiKey, getAPIKey().data(), getAPIKey().size());
    sendMessage(msg);
}

void Client::sendLogout() {
    Message<client::LogoutPayload> msg;
    msg.header = client::makeClientHeader<client::LogoutPayload>(session_);
    msg.payload.serverClientID = session_.serverClientID;
    std::fill(std::begin(msg.payload.padding), std::end(msg.payload.padding), 0x00);
    sendMessage(msg);
}

void Client::sendCancel(OrderID orderID) {
    Message<client::CancelOrderPayload> msg;
    msg.header = client::makeClientHeader<client::CancelOrderPayload>(session_);
    msg.payload.serverClientID = session_.serverClientID;
    msg.payload.serverOrderID = orderID;

    std::fill(std::begin(msg.payload.padding), std::end(msg.payload.padding), 0x00);
    sendMessage(msg);
}

void Client::sendModify(OrderID orderID, Qty newQty, Price newPrice) {
    Message<client::ModifyOrderPayload> msg;
    msg.header = client::makeClientHeader<client::ModifyOrderPayload>(session_);

    msg.payload.serverClientID = session_.serverClientID;
    msg.payload.serverOrderID = orderID;

    msg.payload.newQty = newQty;
    msg.payload.newPrice = newPrice;
    sendMessage(msg);
}

std::list<client::IncomingMessageVariant> Client::processIncoming() {
    std::list<client::IncomingMessageVariant> messages;

    while (true) {
        auto msgOpt = processIncomingMessage_();
        if (!msgOpt) {
            break;
        }

        messages.push_back(std::move(msgOpt.value()));
    }

    return messages;
}

std::optional<client::IncomingMessageVariant> Client::processIncomingMessage_() {
    size_t totalSize = 0;
    std::optional<MessageHeader> headerOpt = peekHeader_();
    if (!headerOpt) {
        return std::nullopt;
    }

    MessageHeader header = headerOpt.value();
    MessageType type = static_cast<MessageType>(header.messageType);

    switch (type) {
    case MessageType::HELLO_ACK: {
        totalSize =
            constants::HEADER_SIZE + server::PayloadTraits<server::HelloAckPayload>::size;
        if (session_.recvBuffer.size() < totalSize) {
            return std::nullopt;
        }

        auto msgOpt = deserializeMessage<server::HelloAckPayload>(session_.recvBuffer);
        if (!msgOpt) {
            return std::nullopt;
        }

        session_.serverSqn = msgOpt.value().header.serverMsgSqn;
        session_.serverClientID = msgOpt.value().payload.serverClientID;

        if (statusCodes::HelloAckStatus::ACCEPTED ==
            static_cast<statusCodes::HelloAckStatus>(msgOpt.value().payload.status)) {
            session_.authenticated = true;
            return makeIncomingVariant_(msgOpt.value(), totalSize);
        } else {
            return makeIncomingVariant_(msgOpt.value(), totalSize);
        }

        break;
    }
    case MessageType::LOGOUT_ACK: {
        totalSize = constants::HEADER_SIZE +
                    server::PayloadTraits<server::LogoutAckPayload>::size;
        if (session_.recvBuffer.size() < totalSize) {
            return std::nullopt;
        }

        auto msgOpt = deserializeMessage<server::LogoutAckPayload>(session_.recvBuffer);

        if (!msgOpt) {
            return std::nullopt;
        }
        session_.clientSqn = msgOpt.value().header.clientMsgSqn;

        if (statusCodes::LogoutAckStatus::ACCEPTED ==
            static_cast<statusCodes::LogoutAckStatus>(msgOpt.value().payload.status)) {
            session_.authenticated = false;
        }
        return makeIncomingVariant_(msgOpt.value(), totalSize);

        break;
    }

    case MessageType::ORDER_ACK: {
        totalSize =
            constants::HEADER_SIZE + server::PayloadTraits<server::OrderAckPayload>::size;
        if (session_.recvBuffer.size() < totalSize) {
            return std::nullopt;
        }
        auto msgOpt = deserializeMessage<server::OrderAckPayload>(session_.recvBuffer);

        if (!msgOpt) {
            return std::nullopt;
        }
        session_.serverSqn = msgOpt.value().header.serverMsgSqn;

        if (auto msg = msgOpt.value();
            msg.payload.status == +statusCodes::OrderAckStatus::ACCEPTED &&
            msg.payload.acceptedPrice != 0) {
            addOutstandingOrder(msg.payload.serverOrderID, msg.payload.acceptedQty,
                                msg.payload.acceptedPrice);
        }

        return makeIncomingVariant_(msgOpt.value(), totalSize);
        break;
    }

    case MessageType::TRADE: {
        totalSize =
            constants::HEADER_SIZE + server::PayloadTraits<server::TradePayload>::size;
        if (session_.recvBuffer.size() < totalSize) {
            return std::nullopt;
        }

        auto msgOpt = deserializeMessage<server::TradePayload>(session_.recvBuffer);

        if (!msgOpt) {
            return std::nullopt;
        }

        session_.serverSqn = msgOpt.value().header.serverMsgSqn;
        session_.exeID++;

        fillOutstandingOrder(msgOpt.value().payload.serverOrderID,
                             msgOpt.value().payload.filledQty);

        return makeIncomingVariant_(msgOpt.value(), totalSize);
        break;
    }

    case MessageType::CANCEL_ACK: {
        totalSize = constants::HEADER_SIZE +
                    server::PayloadTraits<server::CancelAckPayload>::size;
        if (session_.recvBuffer.size() < totalSize) {
            return std::nullopt;
        }

        auto msgOpt = deserializeMessage<server::CancelAckPayload>(session_.recvBuffer);

        if (!msgOpt) {
            return std::nullopt;
        }

        session_.serverSqn = msgOpt.value().header.serverMsgSqn;

        if (auto msg = msgOpt.value();
            msg.payload.status == +statusCodes::CancelAckStatus::ACCEPTED) {
            removeOutstandingOrder(msg.payload.serverOrderID);
        }

        return makeIncomingVariant_(msgOpt.value(), totalSize);
        break;
    }

    case MessageType::MODIFY_ACK: {
        totalSize = constants::HEADER_SIZE +
                    server::PayloadTraits<server::ModifyAckPayload>::size;
        if (session_.recvBuffer.size() < totalSize) {
            return std::nullopt;
        }

        auto msgOpt = deserializeMessage<server::ModifyAckPayload>(session_.recvBuffer);

        if (!msgOpt) {
            return std::nullopt;
        }

        session_.serverSqn = msgOpt.value().header.serverMsgSqn;

        if (auto msg = msgOpt.value();
            msg.payload.status == +statusCodes::ModifyAckStatus::ACCEPTED) {
            modifyOutstandingOrder(msg.payload.oldServerOrderID,
                                   msg.payload.newServerOrderID, msg.payload.newQty,
                                   msg.payload.newPrice);
        }

        return makeIncomingVariant_(msgOpt.value(), totalSize);

        break;
    }

    default: {
        session_.recvBuffer.clear();
        return std::nullopt;
    }
    }
}

std::optional<MessageHeader> Client::peekHeader_() const {
    if (session_.recvBuffer.size() < sizeof(MessageHeader)) {
        return std::nullopt;
    }

    MessageHeader header{};
    std::memcpy(&header, session_.recvBuffer.data(), sizeof(MessageHeader));

    header.payLoadLength = ntohs(header.payLoadLength);
    header.clientMsgSqn = ntohl(header.clientMsgSqn);
    header.serverMsgSqn = ntohl(header.serverMsgSqn);

    return header;
}

void Client::appendRecvBuffer(std::span<const uint8_t> data) {
    session_.recvBuffer.insert(std::end(session_.recvBuffer), std::begin(data),
                               std::end(data));
}

void Client::sendOrder(Qty qty = 100, Price price = 200, bool isBuy = true,
                       bool isLimit = true) {
    Message<client::NewOrderPayload> msg;
    msg.header = client::makeClientHeader<client::NewOrderPayload>(session_);
    msg.payload.serverClientID = session_.serverClientID;
    msg.payload.instrumentID = 1;
    msg.payload.orderSide = isBuy ? +OrderSide::BUY : +OrderSide::SELL;
    msg.payload.orderType = isLimit ? +OrderType::LIMIT : +OrderType::MARKET;
    msg.payload.quantity = qty;
    msg.payload.price = price;
    msg.payload.timeInForce = +(TimeInForce::GTC);
    msg.payload.goodTillDate = std::numeric_limits<Timestamp>::max();

    sendMessage(msg);
}

void Client::sendHeartbeat() {
    Message<client::HeartBeatPayload> msg;
    msg.header = client::makeClientHeader<client::HeartBeatPayload>(session_);
    msg.payload.serverClientID = session_.serverClientID;
    std::fill(std::begin(msg.payload.padding), std::end(msg.payload.padding), 0x00);

    auto serialized = serializeMessage<client::HeartBeatPayload>(MessageType::HEARTBEAT,
                                                                 msg.payload, msg.header);

    session_.sendBuffer.insert(session_.sendBuffer.end(), serialized.begin(),
                               serialized.end());
}

void Client::addOutstandingOrder(OrderID orderID, Qty qty, Price price) {
    OutstandingOrder order{std::chrono::steady_clock::now(), orderID, qty, price};
    outstandingOrders_[orderID] = order;
}

void Client::removeOutstandingOrder(OrderID orderID) {
    auto it = outstandingOrders_.find(orderID);
    if (it == outstandingOrders_.end()) {
        return;
    }
    outstandingOrders_.erase(it);
}

void Client::modifyOutstandingOrder(OrderID orderID, OrderID newOrderID, Qty newQty,
                                    Price newPrice) {
    if (auto it = outstandingOrders_.find(orderID); it != outstandingOrders_.end()) {
        auto node = outstandingOrders_.extract(it);
        node.key() = newOrderID;
        node.mapped().id = newOrderID;
        node.mapped().qty = newQty;
        node.mapped().price = newPrice;

        outstandingOrders_.insert(std::move(node));
    }
}

void Client::fillOutstandingOrder(OrderID orderID, Qty filledQty) {
    if (auto it = outstandingOrders_.find(orderID); it != outstandingOrders_.end()) {
        if ((it->second.qty -= filledQty) == 0) {
            removeOutstandingOrder(orderID);
        }
    }
}

void Client::cancelAll() {
    auto& orders = getOutstandingOrders();

    for (auto& order : orders) {
        sendCancel(order.second.id);
    }
}