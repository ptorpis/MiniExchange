#include "api/api.hpp"
#include "protocol/serialize.hpp"
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
                                                  Message<HelloPayload> msg) {

    std::vector<uint8_t> ack;
    ack.reserve(sizeof(MessageHeader) + sizeof(HelloAckPayload));

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
                                                   Message<LogoutPayload> msg) {
    std::vector<uint8_t> ack;
    ack.reserve(sizeof(MessageHeader) + sizeof(LogoutAckPayload));

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
MiniExchangeAPI::handleNewOrder(Session& session, Message<NewOrderPayload>& msg) {
    std::vector<OutboundMessage> responses;

    OrderRequest req = service_.createRequestFromMessage(msg);

    if (!req.valid) {
        responses.push_back(
            {session.sessionID,
             makeOrderAck_(session, req, std::nullopt, utils::getCurrentTimestampMicros(),
                           statusCodes::OrderAckStatus::INVALID)});
        return responses;
    }

    if (!session.authenticated) {
        responses.push_back(
            {session.sessionID,
             makeOrderAck_(session, req, std::nullopt, utils::getCurrentTimestampMicros(),
                           statusCodes::OrderAckStatus::NOT_AUTHENTICATED)});
        return responses;
    }

    if (session.clientSqn >= msg.header.clientMsgSqn) {
        responses.push_back(
            {session.sessionID,
             makeOrderAck_(session, req, std::nullopt, utils::getCurrentTimestampMicros(),
                           statusCodes::OrderAckStatus::OUT_OF_ORDER)});
        return responses;
    }

    session.clientSqn = msg.header.clientMsgSqn;

    MatchResult result = engine_.processOrder(req);

    responses.push_back(
        {session.sessionID, makeOrderAck_(session, req, result.orderID, result.ts,
                                          statusCodes::OrderAckStatus::ACCEPTED)});

    for (auto& trade : result.tradeVec) {
        Session* buyerSession = getSession(trade.buyerID);
        Session* sellerSession = getSession(trade.sellerID);

        responses.push_back(
            {buyerSession->sessionID, makeTradeMsg_(*buyerSession, trade, true)});
        responses.push_back(
            {sellerSession->sessionID, makeTradeMsg_(*sellerSession, trade, false)});
    }

    return responses;
}

std::vector<uint8_t> MiniExchangeAPI::makeOrderAck_(Session& session, OrderRequest& req,
                                                    std::optional<OrderID> orderID,
                                                    Timestamp ts,
                                                    statusCodes::OrderAckStatus status) {

    auto ackMsg = MessageFactory::makeOrderAck(session, req, orderID, status, ts);
    auto serialized =
        serializeMessage(MessageType::ORDER_ACK, ackMsg.payload, ackMsg.header);
    auto hmac = computeHMAC_(session.hmacKey, serialized.data(),
                             serialized.size() - constants::HMAC_SIZE);
    std::copy(hmac.begin(), hmac.end(), serialized.end() - constants::HMAC_SIZE);
    return serialized;
}

std::vector<uint8_t> MiniExchangeAPI::makeHelloAck_(Session& session,
                                                    statusCodes::HelloStatus status) {
    Message<HelloAckPayload> msg = MessageFactory::makeHelloAck(session, status);
    auto serialized = serializeMessage(MessageType::HELLO_ACK, msg.payload, msg.header);
    auto hmac =
        computeHMAC_(session.hmacKey, serialized.data(), constants::DataSize::HELLO_ACK);
    std::copy(hmac.begin(), hmac.end(),
              serialized.data() + constants::DataSize::HELLO_ACK);
    return serialized;
}

std::vector<uint8_t> MiniExchangeAPI::makeLogoutAck_(Session& session,
                                                     statusCodes::LogoutStatus status) {
    Message<LogoutAckPayload> msg = MessageFactory::makeLoutAck(session, status);
    auto serialized = serializeMessage(MessageType::LOGOUT_ACK, msg.payload, msg.header);
    size_t dataLen = serialized.size() - constants::HMAC_SIZE;
    auto hmac = computeHMAC_(session.hmacKey, serialized.data(), dataLen);
    std::copy(hmac.begin(), hmac.end(), serialized.data() + dataLen);
    return serialized;
}

std::vector<uint8_t> MiniExchangeAPI::makeTradeMsg_(Session& session, TradeEvent& trade,
                                                    bool isBuyer) {
    Message<TradePayload> msg = MessageFactory::makeTradeMsg(session, trade, isBuyer);
    auto serialized =
        serializeMessage<TradePayload>(MessageType::TRADE, msg.payload, msg.header);
    auto hmac =
        computeHMAC_(session.hmacKey, serialized.data(), constants::DataSize::TRADE);
    std::copy(hmac.begin(), hmac.end(), serialized.data() + constants::DataSize::TRADE);
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
