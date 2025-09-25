#include "api/api.hpp"
#include "protocol/serialize.hpp"
#include "protocol/server/serverMessageFactory.hpp"
#include "protocol/server/serverMessages.hpp"
#include "protocol/traits.hpp"
#include <openssl/evp.h>
#include <openssl/hmac.h>

Session& MiniExchangeAPI::connectClient(int fd) {
    return sessionManager_.createSession(fd);
}

void MiniExchangeAPI::disconnectClient(int fd) {
    sessionManager_.removeSession(fd);
}

Session* MiniExchangeAPI::getSession(int fd) {
    return sessionManager_.getSession(fd);
}

std::vector<uint8_t> MiniExchangeAPI::handleHello(Session& session,
                                                  Message<client::HelloPayload> msg) {

    std::vector<uint8_t> ack;
    ack.reserve(sizeof(MessageHeader) + sizeof(server::HelloAckPayload));

    if (session.clientSqn >= msg.header.clientMsgSqn) {
        ack = makeHelloAck_(session, statusCodes::HelloStatus::OUT_OF_ORDER);
        return ack;
    }

    session.clientSqn = msg.header.clientMsgSqn;

    if (!isValidAPIKey_(session, msg.payload.apiKey)) {
        ack = makeHelloAck_(session, statusCodes::HelloStatus::INVALID_API_KEY);
        return ack;
    }

    session.authenticated = true;
    ack = makeHelloAck_(session, statusCodes::HelloStatus::ACCEPTED);
    return ack;
}

std::vector<uint8_t> MiniExchangeAPI::handleLogout(Session& session,
                                                   Message<client::LogoutPayload> msg) {
    std::vector<uint8_t> ack;
    ack.reserve(sizeof(MessageHeader) + sizeof(server::LogoutAckPayload));

    if (session.clientSqn >= msg.header.clientMsgSqn) {
        ack = makeLogoutAck_(session, statusCodes::LogoutStatus::OUT_OF_ORDER);
        return ack;
    }
    session.clientSqn = msg.header.clientMsgSqn;

    session.authenticated = false;
    ack = makeLogoutAck_(session, statusCodes::LogoutStatus::ACCEPTED);
    return ack;
}

std::vector<OutboundMessage>
MiniExchangeAPI::handleNewOrder(Session& session, Message<client::NewOrderPayload>& msg) {
    std::vector<OutboundMessage> responses;

    OrderRequest req = engine_.createRequestFromMessage(msg);

    if (!req.valid) {
        responses.push_back(
            {session.FD,
             makeOrderAck_(session, req, std::nullopt, utils::getCurrentTimestampMicros(),
                           statusCodes::OrderAckStatus::INVALID)});
        return responses;
    }

    if (!session.authenticated) {
        responses.push_back(
            {session.FD,
             makeOrderAck_(session, req, std::nullopt, utils::getCurrentTimestampMicros(),
                           statusCodes::OrderAckStatus::NOT_AUTHENTICATED)});
        return responses;
    }

    if (session.clientSqn >= msg.header.clientMsgSqn) {
        responses.push_back(
            {session.FD,
             makeOrderAck_(session, req, std::nullopt, utils::getCurrentTimestampMicros(),
                           statusCodes::OrderAckStatus::OUT_OF_ORDER)});
        return responses;
    }

    session.clientSqn = msg.header.clientMsgSqn;

    MatchResult result = engine_.processOrder(req);

    responses.push_back(
        {session.FD, makeOrderAck_(session, req, result.orderID, result.ts,
                                   statusCodes::OrderAckStatus::ACCEPTED)});

    for (auto& trade : result.tradeVec) {
        Session* buyerSession = getSession(trade.buyerID);
        Session* sellerSession = getSession(trade.sellerID);

        responses.push_back(
            {buyerSession->FD, makeTradeMsg_(*buyerSession, trade, true)});
        responses.push_back(
            {sellerSession->FD, makeTradeMsg_(*sellerSession, trade, false)});
    }

    return responses;
}

std::vector<uint8_t>
MiniExchangeAPI::handleCancel(Session& session,
                              Message<client::CancelOrderPayload>& msg) {
    std::vector<uint8_t> response;
    if (msg.payload.serverClientID != session.serverClientID) {
        response = makeCancelAck_(session, msg.payload.serverOrderID,
                                  statusCodes::CancelAckStatus::INVALID);
        return response;
    }

    if (!session.authenticated) {
        response = makeCancelAck_(session, msg.payload.serverOrderID,
                                  statusCodes::CancelAckStatus::NOT_AUTHENTICATED);
        return response;
    }

    bool success = engine_.cancelOrder(session.serverClientID, msg.payload.serverOrderID);

    if (success) {
        response = makeCancelAck_(session, msg.payload.serverOrderID,
                                  statusCodes::CancelAckStatus::ACCEPTED);
    } else {
        response = makeCancelAck_(session, msg.payload.serverOrderID,
                                  statusCodes::CancelAckStatus::NOT_FOUND);
    }
    return response;
}

std::vector<OutboundMessage>
MiniExchangeAPI::handleModify(Session& session,
                              Message<client::ModifyOrderPayload>& msg) {
    std::vector<OutboundMessage> responses;

    // new order id is automatically 0 if the status != accepted
    if (msg.payload.serverClientID != session.serverClientID) {
        responses.push_back(
            {session.FD, makeModifyAck_(session, msg.payload.serverOrderID, 0,
                                        statusCodes::ModifyStatus::INVALID)});
    }

    if (!session.authenticated) {
        responses.push_back(
            {session.FD, makeModifyAck_(session, msg.payload.serverOrderID, 0,
                                        statusCodes::ModifyStatus::NOT_AUTHENTICATED)});
    }

    if (session.clientSqn >= msg.header.clientMsgSqn) {
        responses.push_back(
            {session.FD, makeModifyAck_(session, msg.payload.serverOrderID, 0,
                                        statusCodes::ModifyStatus::OUT_OF_ORDER)});
    }

    ModifyResult modResult =
        engine_.modifyOrder(msg.payload.serverClientID, msg.payload.serverOrderID,
                            msg.payload.newQuantity, msg.payload.newPrice);

    if (modResult.event.status == statusCodes::ModifyStatus::ACCEPTED) {
        responses.push_back(
            {session.FD,
             makeModifyAck_(session, msg.payload.serverOrderID,
                            modResult.event.newOrderID, modResult.event.status)});

        if (!modResult.result) {
            return responses;
        }

        for (auto& trade : modResult.result.value().tradeVec) {
            Session* buyerSession = getSession(trade.buyerID);
            Session* sellerSession = getSession(trade.sellerID);

            responses.push_back(
                {buyerSession->FD, makeTradeMsg_(*buyerSession, trade, true)});
            responses.push_back(
                {sellerSession->FD, makeTradeMsg_(*sellerSession, trade, false)});
        }

    } else {

        responses.push_back(
            {session.FD,
             makeModifyAck_(session, modResult.event.oldOrderID,
                            modResult.event.newOrderID, modResult.event.status)});
    }
    return responses;
}

std::vector<uint8_t> MiniExchangeAPI::makeOrderAck_(Session& session, OrderRequest& req,
                                                    std::optional<OrderID> orderID,
                                                    Timestamp ts,
                                                    statusCodes::OrderAckStatus status) {

    auto ackMsg = server::MessageFactory::makeOrderAck(session, req, orderID, status, ts);
    auto serialized =
        serializeMessage(MessageType::ORDER_ACK, ackMsg.payload, ackMsg.header);
    auto hmac = computeHMAC_(session.hmacKey, serialized.data(),
                             serialized.size() - constants::HMAC_SIZE);
    std::copy(hmac.begin(), hmac.end(), serialized.end() - constants::HMAC_SIZE);
    return serialized;
}

std::vector<uint8_t> MiniExchangeAPI::makeHelloAck_(Session& session,
                                                    statusCodes::HelloStatus status) {
    Message<server::HelloAckPayload> msg =
        server::MessageFactory::makeHelloAck(session, status);
    auto serialized = serializeMessage(MessageType::HELLO_ACK, msg.payload, msg.header);
    auto hmac = computeHMAC_(session.hmacKey, serialized.data(),
                             server::PayloadTraits<server::HelloAckPayload>::dataSize);
    std::copy(hmac.begin(), hmac.end(),
              serialized.data() +
                  server::PayloadTraits<server::HelloAckPayload>::dataSize);
    return serialized;
}

std::vector<uint8_t> MiniExchangeAPI::makeLogoutAck_(Session& session,
                                                     statusCodes::LogoutStatus status) {
    Message<server::LogoutAckPayload> msg =
        server::MessageFactory::makeLoutAck(session, status);
    auto serialized = serializeMessage(MessageType::LOGOUT_ACK, msg.payload, msg.header);
    size_t dataLen = serialized.size() - constants::HMAC_SIZE;
    auto hmac = computeHMAC_(session.hmacKey, serialized.data(), dataLen);
    std::copy(hmac.begin(), hmac.end(), serialized.data() + dataLen);
    return serialized;
}

std::vector<uint8_t> MiniExchangeAPI::makeTradeMsg_(Session& session, TradeEvent& trade,
                                                    bool isBuyer) {
    Message<server::TradePayload> msg =
        server::MessageFactory::makeTradeMsg(session, trade, isBuyer);
    auto serialized = serializeMessage<server::TradePayload>(MessageType::TRADE,
                                                             msg.payload, msg.header);
    auto hmac = computeHMAC_(session.hmacKey, serialized.data(),
                             server::PayloadTraits<server::TradePayload>::dataSize);
    std::copy(hmac.begin(), hmac.end(),
              serialized.data() + server::PayloadTraits<server::TradePayload>::dataSize);
    return serialized;
}

std::vector<uint8_t>
MiniExchangeAPI::makeCancelAck_(Session& session, const OrderID orderID,
                                statusCodes::CancelAckStatus status) {
    Message<server::CancelAckPayload> msg =
        server::MessageFactory::makeCancelAck(session, orderID, status);

    auto serialized = serializeMessage<server::CancelAckPayload>(MessageType::CANCEL_ACK,
                                                                 msg.payload, msg.header);
    auto hmac = computeHMAC_(session.hmacKey, serialized.data(),
                             server::PayloadTraits<server::CancelAckPayload>::dataSize);
    std::copy(hmac.begin(), hmac.end(),
              serialized.data() +
                  server::PayloadTraits<server::CancelAckPayload>::dataSize);
    return serialized;
}

std::vector<uint8_t> MiniExchangeAPI::makeModifyAck_(Session& session, OrderID oldOrderID,
                                                     OrderID newOrderID,
                                                     statusCodes::ModifyStatus status) {
    Message<server::ModifyAckPayload> msg =
        server::MessageFactory::makeModifyAck(session, oldOrderID, newOrderID, status);

    auto serialized = serializeMessage<server::ModifyAckPayload>(MessageType::MODIFY_ACK,
                                                                 msg.payload, msg.header);
    auto hmac = computeHMAC_(session.hmacKey, serialized.data(),
                             server::PayloadTraits<server::ModifyAckPayload>::dataSize);
    std::copy(hmac.begin(), hmac.end(),
              serialized.data() +
                  server::PayloadTraits<server::ModifyAckPayload>::dataSize);
    return serialized;
}

bool MiniExchangeAPI::isValidAPIKey_([[maybe_unused]] Session& session,
                                     [[maybe_unused]] const uint8_t key[16]) {
    return true;
}

std::vector<uint8_t> MiniExchangeAPI::computeHMAC_(const std::array<uint8_t, 32>& key,
                                                   const uint8_t* data, size_t dataLen) {
    unsigned int len = 32;

    std::vector<uint8_t> hmac(len);
    HMAC(EVP_sha256(), key.data(), key.size(), data, dataLen, hmac.data(), &len);
    return hmac;
}
