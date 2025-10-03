#include "client/client.hpp"
#include "protocol/client/clientMessageFactory.hpp"
#include "protocol/client/clientMessages.hpp"
#include "protocol/protocolHandler.hpp"
#include "protocol/traits.hpp"
#include "utils/orderBookRenderer.hpp"

#include <gtest/gtest.h>
#include <memory>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <stdexcept>
#include <vector>

class ProtocolHandlingTests : public ::testing::Test {
protected:
    void SetUp() override {
        utils::OrderBookRenderer::enabled = false;
        buyerFD = 1;
        sellerFD = 2;
        buyerClientFD = 3;
        sellerClientFD = 3;

        std::fill(HMACKEY.begin(), HMACKEY.end(), 0x11);
        std::fill(APIKEY.begin(), APIKEY.end(), 0x22);

        handler = std::make_unique<ProtocolHandler>(
            [this](Session& session, const std::span<const uint8_t> buffer) {
                serverCapture.insert(serverCapture.end(), buffer.begin(), buffer.end());

                if (&session == buyerSession) {
                    buyer->appendRecvBuffer(buffer);
                } else if (&session == sellerSession) {
                    seller->appendRecvBuffer(buffer);
                } else {
                    throw std::runtime_error("unkown session");
                }
            });

        buyerSession = &handler->createSession(buyerFD);
        sellerSession = &handler->createSession(sellerFD);

        buyerSession->hmacKey = HMACKEY;
        sellerSession->hmacKey = HMACKEY;

        buyer = std::make_unique<Client>(
            HMACKEY, APIKEY, [this](const std::span<const uint8_t> buffer) {
                clientCapture.insert(clientCapture.end(), buffer.begin(), buffer.end());

                buyerSession->recvBuffer.insert(buyerSession->recvBuffer.end(),
                                                buffer.begin(), buffer.end());
            });

        seller = std::make_unique<Client>(
            HMACKEY, APIKEY, [this](const std::span<const uint8_t> buffer) {
                clientCapture.insert(clientCapture.end(), buffer.begin(), buffer.end());

                sellerSession->recvBuffer.insert(sellerSession->recvBuffer.end(),
                                                 buffer.begin(), buffer.end());
            });

        api = handler->getAPI();
        ASSERT_TRUE(api);
    }

    std::vector<uint8_t> computeHMAC_(const std::array<uint8_t, 32>& key,
                                      const uint8_t* data, size_t dataLen) {
        unsigned int len = 32;

        std::vector<uint8_t> hmac(len);
        HMAC(EVP_sha256(), key.data(), key.size(), data, dataLen, hmac.data(), &len);
        return hmac;
    }

    void login(bool initial = true) {
        resetClientCapture();
        resetServerCapture();

        if (initial) {
            EXPECT_FALSE(buyer->getAuthStatus());
            EXPECT_FALSE(seller->getAuthStatus());
            EXPECT_FALSE(buyerSession->authenticated);
            EXPECT_FALSE(sellerSession->authenticated);
        }

        buyer->sendHello();
        seller->sendHello();

        handler->onMessage(buyerFD);
        handler->onMessage(sellerFD);

        buyer->clearSendBuffer();
        seller->clearSendBuffer();

        buyer->processIncoming();
        seller->processIncoming();

        EXPECT_TRUE(buyer->getAuthStatus());
        EXPECT_TRUE(seller->getAuthStatus());
        EXPECT_TRUE(buyerSession->authenticated);
        EXPECT_TRUE(sellerSession->authenticated);

        clearSendBuffers();
        resetClientCapture();
        resetServerCapture();
    }

    void logout() {
        resetClientCapture();
        resetServerCapture();

        buyer->sendLogout();
        seller->sendLogout();
        handler->onMessage(buyerFD);
        handler->onMessage(sellerFD);
        buyer->clearSendBuffer();
        buyer->processIncoming();

        ASSERT_FALSE(buyer->getAuthStatus());
        ASSERT_FALSE(buyerSession->authenticated);
        ASSERT_FALSE(sellerSession->authenticated);
    }

    void resetServerCapture() { serverCapture.clear(); }
    void resetClientCapture() { clientCapture.clear(); }

    void clearSendBuffers() {
        buyer->clearSendBuffer();
        buyerSession->sendBuffer.clear();
        seller->clearSendBuffer();
        sellerSession->sendBuffer.clear();
    }

    Message<client::NewOrderPayload> testOrderMessage(Qty qty, Price price,
                                                      OrderSide side, OrderType type,
                                                      ClientID clientID = 1) {
        Message<client::NewOrderPayload> msg;

        ClientSession& session =
            clientID == 1 ? buyer->getSession() : seller->getSession();

        msg.header = client::makeClientHeader<client::NewOrderPayload>(session);
        msg.payload.serverClientID = clientID;
        msg.payload.instrumentID = 1;
        msg.payload.orderSide = +(side);
        msg.payload.orderType = +(type);
        msg.payload.quantity = qty;
        msg.payload.price = price;
        msg.payload.timeInForce = +(TimeInForce::GTC);
        msg.payload.goodTillDate = std::numeric_limits<Timestamp>::max();
        std::fill(std::begin(msg.payload.hmac), std::end(msg.payload.hmac), 0x00);
        std::fill(std::begin(msg.payload.padding), std::end(msg.payload.padding), 0x00);

        return msg;
    }

    int buyerFD;
    int sellerFD;
    int buyerClientFD;
    int sellerClientFD;

    std::array<uint8_t, 32> HMACKEY;
    std::array<uint8_t, 16> APIKEY;

    Session* buyerSession;
    Session* sellerSession;
    std::unique_ptr<Client> buyer;
    std::unique_ptr<Client> seller;
    std::unique_ptr<ProtocolHandler> handler;
    std::vector<uint8_t> clientCapture;
    std::vector<uint8_t> serverCapture;

    MiniExchangeAPI* api;
};

TEST_F(ProtocolHandlingTests, BaseCase) {
    login();
    ASSERT_TRUE(buyerSession->authenticated);
    ASSERT_TRUE(buyer->getAuthStatus());
    logout();
}

TEST_F(ProtocolHandlingTests, DoubleLogin) {
    login();
    login(/* is initial */ false);
    ASSERT_EQ(serverCapture.size(), 0);

    logout();
}

TEST_F(ProtocolHandlingTests, LogoutWhenNotAuthenticated) {
    logout();
    ASSERT_EQ(serverCapture.size(), 0);
}

TEST_F(ProtocolHandlingTests, SubmitOrder) {
    login();
    buyer->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(buyerFD);

    ASSERT_EQ(serverCapture.size(),
              server::PayloadTraits<server::OrderAckPayload>::msgSize);

    std::optional<Message<server::OrderAckPayload>> ack =
        deserializeMessage<server::OrderAckPayload>(serverCapture);

    ASSERT_TRUE(ack.has_value());
    ASSERT_EQ(ack.value().header.messageType, +(MessageType::ORDER_ACK));
    ASSERT_EQ(ack.value().payload.acceptedPrice, 200);

    ASSERT_TRUE(api->getBestBid().has_value());
    ASSERT_EQ(api->getBestBid().value(), 200);
    logout();
}

TEST_F(ProtocolHandlingTests, InvalidHMACNewOrder) {
    login();
    std::fill(buyer->getSession().hmacKey.begin(), buyer->getSession().hmacKey.end(),
              0x00);
    buyer->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(buyerFD);
    ASSERT_EQ(serverCapture.size(), 0);
    std::optional<Message<server::OrderAckPayload>> ack =
        deserializeMessage<server::OrderAckPayload>(serverCapture);

    ASSERT_FALSE(ack.has_value());
}

TEST_F(ProtocolHandlingTests, SubmitOrderWithInvalidPrice) {
    login();
    buyer->sendMessage(testOrderMessage(100, 0, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(buyerFD);

    ASSERT_EQ(serverCapture.size(),
              server::PayloadTraits<server::OrderAckPayload>::msgSize);

    std::optional<Message<server::OrderAckPayload>> ack =
        deserializeMessage<server::OrderAckPayload>(serverCapture);

    ASSERT_TRUE(ack.has_value());
    ASSERT_EQ(ack.value().header.messageType, +(MessageType::ORDER_ACK));
    ASSERT_EQ(ack.value().payload.status, +(statusCodes::OrderAckStatus::INVALID));

    ASSERT_FALSE(api->getBestBid().has_value());
    logout();
}

TEST_F(ProtocolHandlingTests, SubmitOrderWithInvalidQty) {
    login();
    buyer->sendMessage(testOrderMessage(0, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(buyerFD);

    ASSERT_EQ(serverCapture.size(),
              server::PayloadTraits<server::OrderAckPayload>::msgSize);

    std::optional<Message<server::OrderAckPayload>> ack =
        deserializeMessage<server::OrderAckPayload>(serverCapture);

    ASSERT_TRUE(ack.has_value());
    ASSERT_EQ(ack.value().header.messageType, +(MessageType::ORDER_ACK));
    ASSERT_EQ(ack.value().payload.status, +(statusCodes::OrderAckStatus::INVALID));

    ASSERT_FALSE(api->getBestBid().has_value());
    logout();
}

TEST_F(ProtocolHandlingTests, SubmitOrderWhenNotAuthenticated) {
    buyer->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(buyerFD);
    ASSERT_EQ(serverCapture.size(), 0);
    std::optional<Message<server::OrderAckPayload>> ack =
        deserializeMessage<server::OrderAckPayload>(serverCapture);

    ASSERT_FALSE(ack.has_value());
}

TEST_F(ProtocolHandlingTests, CancelOrder) {
    login();
    buyer->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(buyerFD);
    std::optional<Message<server::OrderAckPayload>> ack =
        deserializeMessage<server::OrderAckPayload>(serverCapture);
    ASSERT_TRUE(ack.has_value());
    ASSERT_EQ(ack.value().payload.status, +(statusCodes::OrderAckStatus::ACCEPTED));
    clearSendBuffers();
    resetServerCapture();
    resetClientCapture();

    buyer->sendCancel(ack.value().payload.serverOrderID);
    handler->onMessage(buyerFD);

    ASSERT_EQ(serverCapture.size(),
              server::PayloadTraits<server::CancelAckPayload>::msgSize);

    std::optional<Message<server::CancelAckPayload>> cancelAck =
        deserializeMessage<server::CancelAckPayload>(serverCapture);

    ASSERT_TRUE(cancelAck.has_value());
    ASSERT_EQ(cancelAck.value().header.messageType, +(MessageType::CANCEL_ACK));
    ASSERT_EQ(cancelAck.value().payload.status,
              +(statusCodes::CancelAckStatus::ACCEPTED));

    ASSERT_FALSE(api->getBestBid().has_value());
    logout();
}

TEST_F(ProtocolHandlingTests, CancelOrderWhenNotAuthenticated) {
    buyer->sendCancel(1);
    handler->onMessage(buyerFD);
    ASSERT_EQ(serverCapture.size(), 0);
    std::optional<Message<server::CancelAckPayload>> cancelAck =
        deserializeMessage<server::CancelAckPayload>(serverCapture);

    ASSERT_FALSE(cancelAck.has_value());
}

TEST_F(ProtocolHandlingTests, CancelOrderNotFound) {
    login();
    buyer->sendCancel(1);
    handler->onMessage(buyerFD);

    ASSERT_EQ(serverCapture.size(),
              server::PayloadTraits<server::CancelAckPayload>::msgSize);

    std::optional<Message<server::CancelAckPayload>> cancelAck =
        deserializeMessage<server::CancelAckPayload>(serverCapture);

    ASSERT_TRUE(cancelAck.has_value());
    ASSERT_EQ(cancelAck.value().header.messageType, +(MessageType::CANCEL_ACK));
    ASSERT_EQ(cancelAck.value().payload.status,
              +(statusCodes::CancelAckStatus::NOT_FOUND));

    logout();
}

TEST_F(ProtocolHandlingTests, CancelOrderWithInvalidHMAC) {
    login();
    buyer->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(buyerFD);
    resetServerCapture();
    std::fill(buyer->getSession().hmacKey.begin(), buyer->getSession().hmacKey.end(),
              0x00);
    buyer->sendCancel(1);
    handler->onMessage(buyerFD);
    ASSERT_EQ(serverCapture.size(), 0);
    std::optional<Message<server::CancelAckPayload>> cancelAck =
        deserializeMessage<server::CancelAckPayload>(serverCapture);

    ASSERT_FALSE(cancelAck.has_value());
}

TEST_F(ProtocolHandlingTests, ModifyInPlace) {
    login();
    buyer->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(buyerFD);
    std::optional<Message<server::OrderAckPayload>> ack =
        deserializeMessage<server::OrderAckPayload>(serverCapture);
    ASSERT_TRUE(ack.has_value());
    ASSERT_EQ(ack.value().payload.status, +(statusCodes::OrderAckStatus::ACCEPTED));
    clearSendBuffers();
    resetServerCapture();
    resetClientCapture();

    buyer->sendModify(ack.value().payload.serverOrderID, 99, 200);
    handler->onMessage(buyerFD);

    ASSERT_EQ(serverCapture.size(),
              server::PayloadTraits<server::ModifyAckPayload>::msgSize);

    std::optional<Message<server::ModifyAckPayload>> modifyAck =
        deserializeMessage<server::ModifyAckPayload>(serverCapture);

    ASSERT_TRUE(modifyAck.has_value());
    ASSERT_EQ(modifyAck.value().header.messageType, +(MessageType::MODIFY_ACK));
    ASSERT_EQ(modifyAck.value().payload.status, +(statusCodes::ModifyStatus::ACCEPTED));

    ASSERT_TRUE(api->getBestBid().has_value());
    ASSERT_EQ(api->getBestBid().value(), 200);
    ASSERT_EQ(ack.value().payload.serverOrderID,
              modifyAck.value().payload.oldServerOrderID);
    logout();
}

TEST_F(ProtocolHandlingTests, ModifyOrderWhenNotAuthenticated) {
    buyer->sendModify(1, 99, 200);
    handler->onMessage(buyerFD);
    ASSERT_EQ(serverCapture.size(), 0);
    std::optional<Message<server::ModifyAckPayload>> modifyAck =
        deserializeMessage<server::ModifyAckPayload>(serverCapture);

    ASSERT_FALSE(modifyAck.has_value());
}

TEST_F(ProtocolHandlingTests, ModifyOrderNotFound) {
    login();
    buyer->sendModify(1, 99, 200);
    handler->onMessage(buyerFD);

    ASSERT_EQ(serverCapture.size(),
              server::PayloadTraits<server::ModifyAckPayload>::msgSize);

    std::optional<Message<server::ModifyAckPayload>> modifyAck =
        deserializeMessage<server::ModifyAckPayload>(serverCapture);

    ASSERT_TRUE(modifyAck.has_value());
    ASSERT_EQ(modifyAck.value().header.messageType, +(MessageType::MODIFY_ACK));
    ASSERT_EQ(modifyAck.value().payload.status, +(statusCodes::ModifyStatus::NOT_FOUND));

    logout();
}

TEST_F(ProtocolHandlingTests, ModifyOrderWithInvalidHMAC) {
    login();
    buyer->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(buyerFD);
    resetServerCapture();
    std::fill(buyer->getSession().hmacKey.begin(), buyer->getSession().hmacKey.end(),
              0x00);
    buyer->sendModify(1, 99, 200);
    handler->onMessage(buyerFD);
    ASSERT_EQ(serverCapture.size(), 0);
    std::optional<Message<server::ModifyAckPayload>> modifyAck =
        deserializeMessage<server::ModifyAckPayload>(serverCapture);

    ASSERT_FALSE(modifyAck.has_value());
}

TEST_F(ProtocolHandlingTests, ModifyPrice) {
    login();
    buyer->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(buyerFD);
    std::optional<Message<server::OrderAckPayload>> ack =
        deserializeMessage<server::OrderAckPayload>(serverCapture);
    ASSERT_TRUE(ack.has_value());
    ASSERT_EQ(ack.value().payload.status, +(statusCodes::OrderAckStatus::ACCEPTED));
    clearSendBuffers();
    resetServerCapture();
    resetClientCapture();

    buyer->sendModify(ack.value().payload.serverOrderID, 100, 250);
    handler->onMessage(buyerFD);

    ASSERT_EQ(serverCapture.size(),
              server::PayloadTraits<server::ModifyAckPayload>::msgSize);

    std::optional<Message<server::ModifyAckPayload>> modifyAck =
        deserializeMessage<server::ModifyAckPayload>(serverCapture);

    ASSERT_TRUE(modifyAck.has_value());
    ASSERT_EQ(modifyAck.value().header.messageType, +(MessageType::MODIFY_ACK));
    ASSERT_EQ(modifyAck.value().payload.status, +(statusCodes::ModifyStatus::ACCEPTED));

    ASSERT_TRUE(api->getBestBid().has_value());
    ASSERT_EQ(api->getBestBid().value(), 250);
    ASSERT_EQ(ack.value().payload.serverOrderID,
              modifyAck.value().payload.oldServerOrderID);

    ASSERT_NE(modifyAck.value().payload.newServerOrderID,
              modifyAck.value().payload.oldServerOrderID);
    logout();
}

TEST_F(ProtocolHandlingTests, MofifyPriceAndQty) {
    login();
    buyer->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(buyerFD);
    std::optional<Message<server::OrderAckPayload>> ack =
        deserializeMessage<server::OrderAckPayload>(serverCapture);
    ASSERT_TRUE(ack.has_value());
    ASSERT_EQ(ack.value().payload.status, +(statusCodes::OrderAckStatus::ACCEPTED));
    clearSendBuffers();
    resetServerCapture();
    resetClientCapture();

    buyer->sendModify(ack.value().payload.serverOrderID, 50, 250);
    handler->onMessage(buyerFD);

    ASSERT_EQ(serverCapture.size(),
              server::PayloadTraits<server::ModifyAckPayload>::msgSize);

    std::optional<Message<server::ModifyAckPayload>> modifyAck =
        deserializeMessage<server::ModifyAckPayload>(serverCapture);

    ASSERT_TRUE(modifyAck.has_value());
    ASSERT_EQ(modifyAck.value().header.messageType, +(MessageType::MODIFY_ACK));
    ASSERT_EQ(modifyAck.value().payload.status, +(statusCodes::ModifyStatus::ACCEPTED));

    ASSERT_TRUE(api->getBestBid().has_value());
    ASSERT_EQ(api->getBestBid().value(), 250);
    ASSERT_EQ(ack.value().payload.serverOrderID,
              modifyAck.value().payload.oldServerOrderID);

    ASSERT_NE(modifyAck.value().payload.newServerOrderID,
              modifyAck.value().payload.oldServerOrderID);
    logout();
}

TEST_F(ProtocolHandlingTests, FillFromModify) {
    login();
    buyer->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(buyerFD);
    std::optional<Message<server::OrderAckPayload>> ack =
        deserializeMessage<server::OrderAckPayload>(serverCapture);
    ASSERT_TRUE(ack.has_value());
    ASSERT_EQ(ack.value().payload.status, +(statusCodes::OrderAckStatus::ACCEPTED));
    clearSendBuffers();
    resetServerCapture();
    resetClientCapture();

    seller->sendMessage(testOrderMessage(100, 201, OrderSide::SELL, OrderType::LIMIT,
                                         sellerSession->serverClientID));
    handler->onMessage(sellerFD);
    std::optional<Message<server::OrderAckPayload>> sellAck =
        deserializeMessage<server::OrderAckPayload>(serverCapture);
    ASSERT_TRUE(sellAck.has_value());
    ASSERT_EQ(sellAck.value().payload.status, +(statusCodes::OrderAckStatus::ACCEPTED));
    clearSendBuffers();
    resetServerCapture();
    resetClientCapture();

    buyer->sendModify(ack.value().payload.serverOrderID, 100, 201);
    handler->onMessage(buyerFD);
    ASSERT_EQ(serverCapture.size(),
              server::PayloadTraits<server::ModifyAckPayload>::msgSize +
                  server::PayloadTraits<server::TradePayload>::msgSize * 2);

    auto modifyAck = deserializeMessage<server::ModifyAckPayload>(
        std::span(serverCapture)
            .subspan(0, server::PayloadTraits<server::ModifyAckPayload>::msgSize));
    ASSERT_TRUE(modifyAck.has_value());
    ASSERT_EQ(modifyAck.value().header.messageType, +(MessageType::MODIFY_ACK));
    ASSERT_EQ(modifyAck.value().payload.status, +(statusCodes::ModifyStatus::ACCEPTED));
    auto trade1 = deserializeMessage<server::TradePayload>(
        std::span(serverCapture)
            .subspan(server::PayloadTraits<server::ModifyAckPayload>::msgSize));
    ASSERT_TRUE(trade1.has_value());
    ASSERT_EQ(trade1.value().header.messageType, +(MessageType::TRADE));
    ASSERT_EQ(trade1.value().payload.filledQty, 100);
    ASSERT_EQ(trade1.value().payload.filledPrice, 201);

    auto trade2 = deserializeMessage<server::TradePayload>(
        std::span(serverCapture)
            .subspan(server::PayloadTraits<server::ModifyAckPayload>::msgSize +
                     server::PayloadTraits<server::TradePayload>::msgSize));
    ASSERT_TRUE(trade2.has_value());
    ASSERT_EQ(trade2.value().header.messageType, +(MessageType::TRADE));
    ASSERT_EQ(trade2.value().payload.filledQty, 100);
    ASSERT_EQ(trade2.value().payload.filledPrice, 201);
}

TEST_F(ProtocolHandlingTests, MultipleOrders) {
    login();
    for (int i = 0; i < 10; ++i) {
        buyer->sendMessage(
            testOrderMessage(100 + i, 200 + i, OrderSide::BUY, OrderType::LIMIT));
        handler->onMessage(buyerFD);
        resetServerCapture();
    }

    ASSERT_TRUE(api->getBestBid().has_value());
    ASSERT_EQ(api->getBestBid().value(), 209);
    ASSERT_EQ(api->getBidsSize(), 10);
    logout();
}

TEST_F(ProtocolHandlingTests, PartialMessage) {
    login();
    Message<client::NewOrderPayload> msg =
        testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT);
    std::vector<uint8_t> serialized =
        serializeMessage(MessageType::NEW_ORDER, msg.payload, msg.header);

    auto hmac = computeHMAC_(buyer->getSession().hmacKey, serialized.data(),
                             client::PayloadTraits<client::NewOrderPayload>::dataSize);

    std::copy(hmac.begin(), hmac.end(),
              serialized.data() +
                  client::PayloadTraits<client::NewOrderPayload>::hmacOffset);

    size_t half = serialized.size() / 2;

    buyerSession->recvBuffer.insert(buyerSession->recvBuffer.end(), serialized.begin(),
                                    serialized.begin() + half);
    handler->onMessage(buyerFD);
    ASSERT_TRUE(serverCapture.empty());

    buyerSession->recvBuffer.insert(buyerSession->recvBuffer.end(),
                                    serialized.begin() + half, serialized.end());
    handler->onMessage(buyerFD);
    ASSERT_FALSE(serverCapture.empty());

    auto ack = deserializeMessage<server::OrderAckPayload>(serverCapture);
    ASSERT_TRUE(ack.has_value());
    ASSERT_EQ(ack.value().header.messageType, +(MessageType::ORDER_ACK));
}

TEST_F(ProtocolHandlingTests, MultipleMessages) {
    login();
    Message<client::NewOrderPayload> msg1 =
        testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT);
    std::vector<uint8_t> serialized1 =
        serializeMessage(MessageType::NEW_ORDER, msg1.payload, msg1.header);

    auto hmac1 = computeHMAC_(buyer->getSession().hmacKey, serialized1.data(),
                              client::PayloadTraits<client::NewOrderPayload>::dataSize);

    std::copy(hmac1.begin(), hmac1.end(),
              serialized1.data() +
                  client::PayloadTraits<client::NewOrderPayload>::hmacOffset);

    Message<client::NewOrderPayload> msg2 =
        testOrderMessage(50, 250, OrderSide::BUY, OrderType::LIMIT);
    std::vector<uint8_t> serialized2 =
        serializeMessage(MessageType::NEW_ORDER, msg2.payload, msg2.header);

    auto hmac2 = computeHMAC_(buyer->getSession().hmacKey, serialized2.data(),
                              client::PayloadTraits<client::NewOrderPayload>::dataSize);

    std::copy(hmac2.begin(), hmac2.end(),
              serialized2.data() +
                  client::PayloadTraits<client::NewOrderPayload>::hmacOffset);

    buyerSession->recvBuffer.insert(buyerSession->recvBuffer.end(), serialized1.begin(),
                                    serialized1.end());
    buyerSession->recvBuffer.insert(buyerSession->recvBuffer.end(), serialized2.begin(),
                                    serialized2.end());

    handler->onMessage(buyerFD);
    ASSERT_EQ(serverCapture.size(),
              server::PayloadTraits<server::OrderAckPayload>::msgSize * 2);

    auto ack1 = deserializeMessage<server::OrderAckPayload>(
        std::span(serverCapture)
            .subspan(0, server::PayloadTraits<server::OrderAckPayload>::msgSize));
    ASSERT_TRUE(ack1.has_value());
    ASSERT_EQ(ack1.value().header.messageType, +(MessageType::ORDER_ACK));
    ASSERT_EQ(ack1.value().payload.acceptedPrice, 200);

    auto ack2 = deserializeMessage<server::OrderAckPayload>(
        std::span(serverCapture)
            .subspan(server::PayloadTraits<server::OrderAckPayload>::msgSize));
    ASSERT_TRUE(ack2.has_value());
    ASSERT_EQ(ack2.value().header.messageType, +(MessageType::ORDER_ACK));
    ASSERT_EQ(ack2.value().payload.acceptedPrice, 250);
}

TEST_F(ProtocolHandlingTests, UnknownMessageType) {
    login();
    std::vector<uint8_t> unknownMsg(64, 0xFF);
    buyerSession->recvBuffer.insert(buyerSession->recvBuffer.end(), unknownMsg.begin(),
                                    unknownMsg.end());
    handler->onMessage(buyerFD);
    ASSERT_FALSE(api->getSession(buyerFD));
    ASSERT_TRUE(buyerSession->recvBuffer.empty());
    ASSERT_TRUE(serverCapture.empty());
}

TEST_F(ProtocolHandlingTests, EmptyMessage) {
    login();
    std::vector<uint8_t> emptyMsg;
    buyerSession->recvBuffer.insert(buyerSession->recvBuffer.end(), emptyMsg.begin(),
                                    emptyMsg.end());
    handler->onMessage(buyerFD);
    ASSERT_TRUE(api->getSession(buyerFD));
    ASSERT_TRUE(buyerSession->recvBuffer.empty());
    ASSERT_TRUE(serverCapture.empty());
}

TEST_F(ProtocolHandlingTests, ValidMessageAfterBadMessage) {
    login();
    std::vector<uint8_t> unknownMsg(64, 0xFF);
    buyerSession->recvBuffer.insert(buyerSession->recvBuffer.end(), unknownMsg.begin(),
                                    unknownMsg.end());
    handler->onMessage(buyerFD);
    ASSERT_TRUE(buyerSession->recvBuffer.empty());
    ASSERT_TRUE(serverCapture.empty());

    ASSERT_FALSE(api->getSession(buyerFD));

    // client should be dropped due to previous bad message
    ASSERT_TRUE(serverCapture.empty());
}

TEST_F(ProtocolHandlingTests, UnsupportedProtocolVersion) {
    login();
    Message<client::NewOrderPayload> msg =
        testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT);
    msg.header.protocolVersionFlag = 0xFF; // unsupported version
    buyer->sendMessage<client::NewOrderPayload>(msg);
    handler->onMessage(buyerFD);
    ASSERT_FALSE(api->getSession(buyerFD));
    ASSERT_TRUE(buyerSession->recvBuffer.empty());
    ASSERT_TRUE(serverCapture.empty());
}