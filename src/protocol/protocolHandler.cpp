#include "protocol/protocolHandler.hpp"
#include "protocol/clientMessages.hpp"
#include "protocol/messages.hpp"
#include "protocol/serialize.hpp"
#include "protocol/serverMessages.hpp"
#include "utils/status.hpp"
#include "utils/timing.hpp"
#include "utils/types.hpp"
#include "utils/utils.hpp"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <optional>

void ProtocolHandler::onMessage(int fd) {
    Session* session = sessionManager_.getSession(fd);

    if (!session) {
        return;
    }

    processMessages_(*session);
}

void ProtocolHandler::processMessages_(Session& session) {
    std::span<const std::byte> view = session.recvBuffer;
    std::size_t totalConsumed{0};

    while (!view.empty()) {
        if (view.size() < sizeof(MessageHeader)) {
            break;
        }

        auto header = peekHeader_(view);
        if (!header) {
            break;
        }

        std::size_t totalMessageSize = sizeof(MessageHeader) + header->payloadLength;

        if (view.size() < totalMessageSize) {
            break;
        }

        std::span<const std::byte> messageBytes = view.subspan(0, totalMessageSize);
        std::size_t consumed = handleMessage_(session, messageBytes);

        if (consumed == 0) {
            break;
        }

        view = view.subspan(consumed);
        totalConsumed += consumed;
    }

    if (totalConsumed > 0) {
        auto newEnd =
            std::shift_left(session.recvBuffer.begin(), session.recvBuffer.end(),
                            static_cast<std::ptrdiff_t>(totalConsumed));
        session.recvBuffer.erase(newEnd, session.recvBuffer.end());
    }
}

std::size_t ProtocolHandler::handleMessage_(Session& session,
                                            std::span<const std::byte> messageBytes) {
    MessageType type = static_cast<MessageType>(messageBytes[0]);

    switch (type) {
    case MessageType::HELLO: {
        return handleHello_(session, messageBytes);
    }
    case MessageType::LOGOUT: {
        return handleLogout_(session, messageBytes);
    }
    case MessageType::NEW_ORDER: {
        return handleNewOrder_(session, messageBytes);
    }
    case MessageType::CANCEL_ORDER: {
        return handleCancel_(session, messageBytes);
    }
    case MessageType::MODIFY_ORDER: {
        return handleModifyOrder_(session, messageBytes);
    }

    default: {
        return 0;
    }
    }
}

std::optional<MessageHeader>
ProtocolHandler::peekHeader_(std::span<const std::byte> view) const {
    if (view.size() < sizeof(MessageHeader)) {
        return std::nullopt;
    }

    MessageHeader header;
    std::span<const std::byte> headerView = view;

    header.messageType = readByteAdvance(headerView);
    header.protocolVersionFlag = readByteAdvance(headerView);
    header.payloadLength = readIntegerAdvance<std::uint16_t>(headerView);
    header.clientMsgSqn = readIntegerAdvance<std::uint32_t>(headerView);
    header.serverMsgSqn = readIntegerAdvance<std::uint32_t>(headerView);
    readBytesAdvance(headerView, reinterpret_cast<std::byte*>(header.padding),
                     sizeof(header.padding));

    return header;
}

std::size_t ProtocolHandler::handleHello_(Session& session,
                                          std::span<const std::byte> messageBytes) {
    constexpr std::size_t sizeToBeConsumed =
        MessageHeader::traits::HEADER_SIZE + client::HelloPayload::traits::payloadSize;

    if (session.recvBuffer.size() < sizeToBeConsumed) {
        return 0;
    }

    if (session.isAuthenticated()) {
        return sizeToBeConsumed;
    }

    if (auto msgOpt = deserializeMessage<client::HelloPayload>(messageBytes)) {
        if (!utils::isCorrectIncrement(session.getClientSqn().value(),
                                       msgOpt->header.clientMsgSqn)) {
            return sizeToBeConsumed;
        }
        sessionManager_.authenticateClient(session.fd);
    } else {
        return sizeToBeConsumed;
    }

    Message<server::HelloAckPayload> ackMsg =
        makeHelloAck_(session, status::HelloAckStatus::ACCEPTED);

    serializeMessageInto(session.sendBuffer, MessageType::HELLO_ACK, ackMsg.header,
                         ackMsg.payload);
    dirtyFDs_.insert(session.fd);

    return sizeToBeConsumed;
}

std::size_t ProtocolHandler::handleLogout_(Session& session,
                                           std::span<const std::byte> messageBytes) {
    constexpr std::size_t sizeToBeConsumed =
        client::LogoutPayload::traits::payloadSize + MessageHeader::traits::HEADER_SIZE;
    if (session.recvBuffer.size() < sizeToBeConsumed) {
        return 0;
    }

    if (!session.isAuthenticated()) {
        return sizeToBeConsumed;
    }

    if (auto msgOpt = deserializeMessage<client::LogoutPayload>(messageBytes)) {
        if (!utils::isCorrectIncrement(session.getClientSqn().value(),
                                       msgOpt->header.clientMsgSqn)) {
            return sizeToBeConsumed;
        }
        sessionManager_.logoutClient(session.fd);
    } else {
        return sizeToBeConsumed;
    }

    Message<server::LogoutAckPayload> ackMsg =
        makeLogoutAck_(session, status::LogoutAckStatus::ACCEPTED);

    serializeMessageInto(session.sendBuffer, MessageType::LOGOUT_ACK, ackMsg.header,
                         ackMsg.payload);
    dirtyFDs_.insert(session.fd);

    return sizeToBeConsumed;
}

std::size_t ProtocolHandler::handleNewOrder_(Session& session,
                                             std::span<const std::byte> messageBytes) {
    constexpr std::size_t sizeToBeConsumed =
        client::NewOrderPayload::traits::payloadSize + MessageHeader::traits::HEADER_SIZE;

    if (session.recvBuffer.size() < sizeToBeConsumed) {
        return 0;
    }

    if (!session.isAuthenticated()) {
        return sizeToBeConsumed;
    }

    if (auto msgOpt = deserializeMessage<client::NewOrderPayload>(messageBytes)) {
        if (!utils::isCorrectIncrement(session.getClientSqn().value(),
                                       msgOpt->header.clientMsgSqn)) {
            return sizeToBeConsumed;
        }
        MatchResult result = api_.processNewOrder(msgOpt->payload);

        Message<server::OrderAckPayload> ackMsg =
            makeOrderAck_(session, result, ClientOrderID{msgOpt->payload.clientOrderID});

        serializeMessageInto(session.sendBuffer, MessageType::ORDER_ACK, ackMsg.header,
                             ackMsg.payload);
        dirtyFDs_.insert(session.fd);

        for (auto& trade : result.tradeVec) {
            Session* buyerSession = sessionManager_.getSession(trade.buyerID);
            if (buyerSession) {
                bool isBuyer = true;

                auto tradeMsg = makeTradeMsg_(*buyerSession, trade, isBuyer);

                serializeMessageInto(buyerSession->sendBuffer, MessageType::TRADE,
                                     tradeMsg.header, tradeMsg.payload);
                dirtyFDs_.insert(buyerSession->fd);
            }

            Session* sellerSession = sessionManager_.getSession(trade.sellerID);
            if (sellerSession) {
                bool isBuyer = false;
                auto tradeMsg = makeTradeMsg_(*sellerSession, trade, isBuyer);
                serializeMessageInto(sellerSession->sendBuffer, MessageType::TRADE,
                                     tradeMsg.header, tradeMsg.payload);
                dirtyFDs_.insert(sellerSession->fd);
            }
        }
    } else {
        return 0;
    }

    return sizeToBeConsumed;
}

std::size_t ProtocolHandler::handleModifyOrder_(Session& session,
                                                std::span<const std::byte> messageBytes) {
    constexpr std::size_t sizeToBeConsumed =
        MessageHeader::traits::HEADER_SIZE +
        client::ModifyOrderPayload::traits::payloadSize;

    if (session.recvBuffer.size() < sizeToBeConsumed) {
        return 0;
    }

    if (auto msgOpt = deserializeMessage<client::ModifyOrderPayload>(messageBytes)) {
        if (!utils::isCorrectIncrement(session.getClientSqn().value(),
                                       msgOpt->header.clientMsgSqn)) {
            return sizeToBeConsumed;
        }
        ModifyResult res = api_.modifyOrder(msgOpt->payload);

        auto ackMsg = makeModifyAck_(session, res);
        serializeMessageInto(session.sendBuffer, MessageType::MODIFY_ACK, ackMsg.header,
                             ackMsg.payload);
        dirtyFDs_.insert(session.fd);

        if (res.matchResult) {
            for (auto& trade : res.matchResult->tradeVec) {
                Session* buyerSession = sessionManager_.getSession(trade.buyerID);
                if (buyerSession) {
                    bool isBuyer = true;
                    auto tradeMsg = makeTradeMsg_(*buyerSession, trade, isBuyer);
                    serializeMessageInto(buyerSession->sendBuffer, MessageType::TRADE,
                                         tradeMsg.header, tradeMsg.payload);
                    dirtyFDs_.insert(buyerSession->fd);
                }
                Session* sellerSession = sessionManager_.getSession(trade.sellerID);
                if (sellerSession) {
                    bool isBuyer = false;
                    auto tradeMsg = makeTradeMsg_(*sellerSession, trade, isBuyer);
                    serializeMessageInto(sellerSession->sendBuffer, MessageType::TRADE,
                                         tradeMsg.header, tradeMsg.payload);
                    dirtyFDs_.insert(sellerSession->fd);
                }
            }
        }
    }

    return sizeToBeConsumed;
}

std::size_t ProtocolHandler::handleCancel_(Session& session,
                                           std::span<const std::byte> messageBytes) {
    constexpr std::size_t sizeToBeConsumed =
        MessageHeader::traits::HEADER_SIZE +
        client::CancelOrderPayload::traits::payloadSize;

    if (session.recvBuffer.size() < sizeToBeConsumed) {
        return 0;
    }

    if (auto msgOpt = deserializeMessage<client::CancelOrderPayload>(messageBytes)) {
        if (!utils::isCorrectIncrement(session.getClientSqn().value(),
                                       msgOpt->header.clientMsgSqn)) {
            return sizeToBeConsumed;
        }

        bool success = api_.cancelOrder(msgOpt->payload);

        Message<server::CancelAckPayload> ackMsg =
            makeCancelAck_(session, OrderID{msgOpt->payload.serverOrderID},
                           ClientOrderID{msgOpt->payload.clientOrderID},
                           InstrumentID{msgOpt->payload.instrumentID}, success);

        serializeMessageInto(session.sendBuffer, MessageType::CANCEL_ACK, ackMsg.header,
                             ackMsg.payload);
        dirtyFDs_.insert(session.fd);
    }

    return sizeToBeConsumed;
}

template <typename Payload> inline MessageHeader makeHeader(Session& session) {
    MessageHeader header{};
    header.messageType = +Payload::traits::type;
    header.protocolVersionFlag = +(MessageHeader::traits::PROTOCOL_VERSION);
    header.payloadLength = Payload::traits::payloadSize;
    header.clientMsgSqn = session.getClientSqn().value();
    header.serverMsgSqn = session.getNextClientSqn().value();
    std::memset(header.padding, 0, sizeof(header.padding));

    return header;
}

Message<server::HelloAckPayload>
ProtocolHandler::makeHelloAck_(Session& session, status::HelloAckStatus statusCode) {
    Message<server::HelloAckPayload> msg;
    msg.header = makeHeader<server::HelloAckPayload>(session);
    msg.payload.serverClientID = session.getClientID().value();
    msg.payload.status = +statusCode;
    std::memset(msg.payload.padding, 0, sizeof(msg.payload.padding));

    return msg;
}

Message<server::LogoutAckPayload>
ProtocolHandler::makeLogoutAck_(Session& session, status::LogoutAckStatus statusCode) {
    Message<server::LogoutAckPayload> msg;
    msg.header = makeHeader<server::LogoutAckPayload>(session);
    msg.payload.serverClientID = session.getClientID().value();
    msg.payload.status = +statusCode;
    std::memset(msg.payload.padding, 0, sizeof(msg.payload.padding));

    return msg;
}

Message<server::OrderAckPayload>
ProtocolHandler::makeOrderAck_(Session& session, const MatchResult& result,
                               ClientOrderID clientOrderID) {
    Message<server::OrderAckPayload> msg;
    msg.header = makeHeader<server::OrderAckPayload>(session);
    msg.payload.serverClientID = session.getClientID().value();
    msg.payload.serverOrderID = result.orderID.value();
    msg.payload.serverOrderID = clientOrderID.value();
    msg.payload.acceptedPrice = result.acceptedPrice.value();
    msg.payload.remainingQty = result.remainingQty.value();
    msg.payload.serverTime = TSCClock::now();
    msg.payload.instrumentID = result.instrumentID.value();
    msg.payload.status = +result.status;

    std::memset(msg.payload.padding, 0, sizeof(msg.payload.padding));
    return msg;
}

Message<server::TradePayload>
ProtocolHandler::makeTradeMsg_(Session& session, const TradeEvent& ev, bool isBuyer) {
    Message<server::TradePayload> msg;

    msg.header = makeHeader<server::TradePayload>(session);
    msg.payload.serverClientID = session.getClientID().value();

    msg.payload.serverOrderID =
        isBuyer ? ev.buyerOrderID.value() : ev.sellerOrderID.value();
    msg.payload.clientOrderID =
        isBuyer ? ev.buyerClientOrderID.value() : ev.sellerClientOrderID.value();
    msg.payload.tradeID = ev.tradeID.value();
    msg.payload.filledQty = ev.qty.value();
    msg.payload.filledPrice = ev.price.value();
    msg.payload.timestamp = TSCClock::now();

    return msg;
}

Message<server::ModifyAckPayload>
ProtocolHandler::makeModifyAck_(Session& session, const ModifyResult& res,
                                ClientOrderID clientOrderID) {
    Message<server::ModifyAckPayload> msg;

    msg.header = makeHeader<server::ModifyAckPayload>(session);
    msg.payload.serverClientID = session.getClientID().value();
    msg.payload.oldServerOrderID = res.oldOrderID.value();
    msg.payload.newServerOrderID = res.newOrderID.value();
    msg.payload.clientOrderID = clientOrderID.value();
    msg.payload.newQty = res.newQty.value();
    msg.payload.newPrice = res.newPrice.value();
    msg.payload.status = +res.status;

    std::memset(msg.payload.padding, 0, sizeof(msg.payload.padding));

    return msg;
}

Message<server::CancelAckPayload>
ProtocolHandler::makeCancelAck_(Session& session, OrderID orderID,
                                ClientOrderID clientOrderID, InstrumentID instrID,
                                bool success) {
    Message<server::CancelAckPayload> msg;
    msg.header = makeHeader<server::CancelAckPayload>(session);

    msg.payload.serverClientID = session.getClientID().value();
    msg.payload.serverOrderID = orderID.value();
    msg.payload.clientOrderID = clientOrderID.value();
    msg.payload.instrumentID = instrID.value();
    msg.payload.status =
        success ? +status::CancelStatus::ACCEPTED : +status::CancelStatus::REJECTED;

    std::memset(msg.payload.padding, 0, sizeof(msg.payload.padding));

    return msg;
}
