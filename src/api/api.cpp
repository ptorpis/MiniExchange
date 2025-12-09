#include "api/api.hpp"
#include "protocol/serialize.hpp"
#include "protocol/server/serverMessageFactory.hpp"
#include "protocol/server/serverMessages.hpp"

Session& MiniExchangeAPI::connectClient(int fd) {
    return sessionManager_.createSession(fd);
}

void MiniExchangeAPI::disconnectClient(int fd) {
    Session* session = sessionManager_.getSession(fd);
    if (!session) return;
    session->reset();
    sessionManager_.removeSession(fd);
}

Session* MiniExchangeAPI::getSession(int fd) {
    return sessionManager_.getSession(fd);
}

std::vector<uint8_t> MiniExchangeAPI::handleHello(Session& session,
                                                  Message<client::HelloPayload> msg) {

    std::vector<uint8_t> ack;
    ack.reserve(sizeof(MessageHeader) + sizeof(server::HelloAckPayload));

    if (!utils::isCorrectIncrement(msg.header.clientMsgSqn, session.clientSqn)) {
        ack = makeHelloAck_(session, statusCodes::HelloAckStatus::OUT_OF_ORDER);
        return ack;
    }

    session.clientSqn = msg.header.clientMsgSqn;

    if (!isValidAPIKey_(session, msg.payload.apiKey)) {
        ack = makeHelloAck_(session, statusCodes::HelloAckStatus::INVALID_API_KEY);
        return ack;
    }

    session.authenticated = true;
    ack = makeHelloAck_(session, statusCodes::HelloAckStatus::ACCEPTED);
    return ack;
}

std::vector<uint8_t> MiniExchangeAPI::handleLogout(Session& session,
                                                   Message<client::LogoutPayload> msg) {
    std::vector<uint8_t> ack;
    ack.reserve(sizeof(MessageHeader) + sizeof(server::LogoutAckPayload));

    if (!utils::isCorrectIncrement(msg.header.clientMsgSqn, session.clientSqn)) {
        ack = makeLogoutAck_(session, statusCodes::LogoutAckStatus::OUT_OF_ORDER);
        return ack;
    }
    session.clientSqn = msg.header.clientMsgSqn;

    session.authenticated = false;
    ack = makeLogoutAck_(session, statusCodes::LogoutAckStatus::ACCEPTED);
    return ack;
}

std::vector<OutboundMessage>
MiniExchangeAPI::handleNewOrder(Session& session, Message<client::NewOrderPayload>& msg) {
    std::vector<OutboundMessage> responses;

    OrderRequest req = engine_.createRequestFromMessage(msg);

    if (!req.valid) {
        responses.push_back(
            {session.FD, makeOrderAck_(session, req, std::nullopt,
                                       statusCodes::OrderAckStatus::INVALID)});
        return responses;
    }

    if (!session.authenticated) {
        responses.push_back(
            {session.FD, makeOrderAck_(session, req, std::nullopt,
                                       statusCodes::OrderAckStatus::NOT_AUTHENTICATED)});
        return responses;
    }

    if (!utils::isCorrectIncrement(msg.header.clientMsgSqn, session.clientSqn)) {
        responses.push_back(
            {session.FD, makeOrderAck_(session, req, std::nullopt,
                                       statusCodes::OrderAckStatus::OUT_OF_ORDER)});
        return responses;
    }

    session.clientSqn = msg.header.clientMsgSqn;

    MatchResult result = engine_.processOrder(req);

    responses.push_back(
        {session.FD, makeOrderAck_(session, req, result.orderID,
                                   statusCodes::OrderAckStatus::ACCEPTED)});

    for (auto& trade : result.tradeVec) {
        Session* buyerSession = sessionManager_.getSessionFromClientID(trade.buyerID);
        if (buyerSession) {
            responses.push_back(
                {buyerSession->FD, makeTradeMsg_(*buyerSession, trade, true)});
        }

        Session* sellerSession = sessionManager_.getSessionFromClientID(trade.sellerID);
        if (sellerSession) {
            responses.push_back(
                {sellerSession->FD, makeTradeMsg_(*sellerSession, trade, false)});
        }
    }

    return responses;
}

std::vector<uint8_t>
MiniExchangeAPI::handleCancel(Session& session,
                              Message<client::CancelOrderPayload>& msg) {
    std::vector<uint8_t> response;
    if (!utils::isCorrectIncrement(msg.header.clientMsgSqn, session.clientSqn)) {
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

    session.clientSqn = msg.header.clientMsgSqn;

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
                                        msg.payload.newQty, msg.payload.newPrice,
                                        statusCodes::ModifyAckStatus::INVALID)});
    }

    if (!session.authenticated) {
        responses.push_back(
            {session.FD,
             makeModifyAck_(session, msg.payload.serverOrderID, 0, msg.payload.newQty,
                            msg.payload.newPrice,
                            statusCodes::ModifyAckStatus::NOT_AUTHENTICATED)});
    }

    if (!utils::isCorrectIncrement(msg.header.clientMsgSqn, session.clientSqn)) {
        responses.push_back(
            {session.FD, makeModifyAck_(session, msg.payload.serverOrderID, 0,
                                        msg.payload.newQty, msg.payload.newPrice,
                                        statusCodes::ModifyAckStatus::OUT_OF_ORDER)});
    }

    ModifyResult modResult =
        engine_.modifyOrder(msg.payload.serverClientID, msg.payload.serverOrderID,
                            msg.payload.newQty, msg.payload.newPrice);

    session.clientSqn = msg.header.clientMsgSqn;

    if (modResult.event.status == statusCodes::ModifyAckStatus::ACCEPTED) {
        responses.push_back(
            {session.FD, makeModifyAck_(session, msg.payload.serverOrderID,
                                        modResult.event.newOrderID, msg.payload.newQty,
                                        msg.payload.newPrice, modResult.event.status)});

        if (!modResult.result) {
            return responses;
        }

        for (auto& trade : modResult.result.value().tradeVec) {
            Session* buyerSession = sessionManager_.getSessionFromClientID(trade.buyerID);
            if (buyerSession) {

                responses.push_back(
                    {buyerSession->FD, makeTradeMsg_(*buyerSession, trade, true)});
            }
            Session* sellerSession =
                sessionManager_.getSessionFromClientID(trade.sellerID);
            if (sellerSession) {
                responses.push_back(
                    {sellerSession->FD, makeTradeMsg_(*sellerSession, trade, false)});
            }
        }

    } else {

        responses.push_back(
            {session.FD, makeModifyAck_(session, modResult.event.oldOrderID,
                                        modResult.event.newOrderID, msg.payload.newQty,
                                        msg.payload.newPrice, modResult.event.status)});
    }
    return responses;
}

std::vector<uint8_t> MiniExchangeAPI::makeOrderAck_(Session& session, OrderRequest& req,
                                                    std::optional<OrderID> orderID,
                                                    statusCodes::OrderAckStatus status) {

    auto ackMsg = server::MessageFactory::makeOrderAck(session, req, orderID, status);
    auto serialized =
        serializeMessage(MessageType::ORDER_ACK, ackMsg.payload, ackMsg.header);
    return serialized;
}

std::vector<uint8_t> MiniExchangeAPI::makeHelloAck_(Session& session,
                                                    statusCodes::HelloAckStatus status) {
    Message<server::HelloAckPayload> msg =
        server::MessageFactory::makeHelloAck(session, status);
    auto serialized = serializeMessage(MessageType::HELLO_ACK, msg.payload, msg.header);
    return serialized;
}

std::vector<uint8_t>
MiniExchangeAPI::makeLogoutAck_(Session& session, statusCodes::LogoutAckStatus status) {
    Message<server::LogoutAckPayload> msg =
        server::MessageFactory::makeLoutAck(session, status);
    auto serialized = serializeMessage(MessageType::LOGOUT_ACK, msg.payload, msg.header);
    return serialized;
}

std::vector<uint8_t> MiniExchangeAPI::makeTradeMsg_(Session& session, TradeEvent& trade,
                                                    bool isBuyer) {
    Message<server::TradePayload> msg =
        server::MessageFactory::makeTradeMsg(session, trade, isBuyer);
    auto serialized = serializeMessage<server::TradePayload>(MessageType::TRADE,
                                                             msg.payload, msg.header);
    return serialized;
}

std::vector<uint8_t>
MiniExchangeAPI::makeCancelAck_(Session& session, const OrderID orderID,
                                statusCodes::CancelAckStatus status) {
    Message<server::CancelAckPayload> msg =
        server::MessageFactory::makeCancelAck(session, orderID, status);

    auto serialized = serializeMessage<server::CancelAckPayload>(MessageType::CANCEL_ACK,
                                                                 msg.payload, msg.header);
    return serialized;
}

std::vector<uint8_t>
MiniExchangeAPI::makeModifyAck_(Session& session, OrderID oldOrderID, OrderID newOrderID,
                                Qty newQty, Price newPrice,
                                statusCodes::ModifyAckStatus status) {
    Message<server::ModifyAckPayload> msg = server::MessageFactory::makeModifyAck(
        session, oldOrderID, newOrderID, newQty, newPrice, status);

    auto serialized = serializeMessage<server::ModifyAckPayload>(MessageType::MODIFY_ACK,
                                                                 msg.payload, msg.header);
    return serialized;
}

bool MiniExchangeAPI::isValidAPIKey_([[maybe_unused]] Session& session,
                                     [[maybe_unused]] const uint8_t key[16]) {
    return true;
}
