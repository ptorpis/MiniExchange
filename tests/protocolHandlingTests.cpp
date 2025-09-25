#include "client/client.hpp"
#include "protocol/client/clientMessageFactory.hpp"
#include "protocol/client/clientMessages.hpp"
#include "protocol/protocolHandler.hpp"
#include "protocol/traits.hpp"
#include <arpa/inet.h>
#include <array>
#include <gtest/gtest.h>
#include <memory>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <stdexcept>
#include <vector>

class ProtocolHandlingTests : public ::testing::Test {
protected:
    void SetUp() override {
        serverFD = 1;
        clientFD = 2;

        std::fill(HMACKEY.begin(), HMACKEY.end(), 0x11);
        std::fill(APIKEY.begin(), APIKEY.end(), 0x22);

        handler = std::make_unique<ProtocolHandler>(
            [this]([[maybe_unused]] Session& session,
                   const std::span<const uint8_t> buffer) {
                serverCapture.insert(serverCapture.end(), buffer.begin(), buffer.end());

                if (client) {
                    client->appendRecvBuffer(buffer);
                } else {
                    throw std::runtime_error("client is null");
                }
            });
        serverSession = &handler->createSession(serverFD);
        serverSession->hmacKey = HMACKEY;

        client = std::make_unique<Client>(
            HMACKEY, APIKEY, clientFD, [this](const std::span<const uint8_t> buffer) {
                clientCapture.insert(clientCapture.end(), buffer.begin(), buffer.end());

                serverSession->recvBuffer.insert(serverSession->recvBuffer.end(),
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
            EXPECT_FALSE(client->getAuthStatus());
            EXPECT_FALSE(serverSession->authenticated);
        }
        client->sendHello();

        handler->onMessage(serverFD);
        client->clearSendBuffer();

        client->processIncoming();
        serverSession->sendBuffer.clear();

        EXPECT_TRUE(client->getAuthStatus());
        EXPECT_TRUE(serverSession->authenticated);
        clearSendBuffers();
        resetClientCapture();
        if (initial) {
            resetServerCapture();
        }
    }

    void logout() {
        resetClientCapture();
        resetServerCapture();

        client->sendLogout();
        handler->onMessage(serverFD);
        client->clearSendBuffer();
        client->processIncoming();

        ASSERT_FALSE(client->getAuthStatus());
        ASSERT_FALSE(serverSession->authenticated);
    }

    void resetServerCapture() { serverCapture.clear(); }
    void resetClientCapture() { clientCapture.clear(); }

    void clearSendBuffers() {
        client->clearSendBuffer();
        serverSession->sendBuffer.clear();
    }

    Message<client::NewOrderPayload> testOrderMessage(Qty qty, Price price,
                                                      OrderSide side, OrderType type) {
        Message<client::NewOrderPayload> msg;
        msg.header =
            client::makeClientHeader<client::NewOrderPayload>(client->getSession());
        msg.payload.serverClientID = client->getSession().serverClientID;
        msg.payload.instrumentID = 1;
        msg.payload.orderSide = std::to_underlying(side);
        msg.payload.orderType = std::to_underlying(type);
        msg.payload.quantity = qty;
        msg.payload.price = price;
        msg.payload.timeInForce = std::to_underlying(TimeInForce::GTC);
        msg.payload.goodTillDate = std::numeric_limits<Timestamp>::max();
        std::fill(std::begin(msg.payload.hmac), std::end(msg.payload.hmac), 0x00);
        std::fill(std::begin(msg.payload.padding), std::end(msg.payload.padding), 0x00);

        return msg;
    }

    int serverFD;
    int clientFD;

    std::array<uint8_t, 32> HMACKEY;
    std::array<uint8_t, 16> APIKEY;

    Session* serverSession;
    std::unique_ptr<Client> client;
    std::unique_ptr<ProtocolHandler> handler;
    std::vector<uint8_t> clientCapture;
    std::vector<uint8_t> serverCapture;

    MiniExchangeAPI* api;
};

TEST_F(ProtocolHandlingTests, BaseCase) {
    login();
    ASSERT_TRUE(serverSession->authenticated);
    ASSERT_TRUE(client->getAuthStatus());
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
    client->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(serverFD);

    ASSERT_EQ(serverCapture.size(),
              server::PayloadTraits<server::OrderAckPayload>::msgSize);

    std::optional<Message<server::OrderAckPayload>> ack =
        deserializeMessage<server::OrderAckPayload>(serverCapture);

    ASSERT_TRUE(ack.has_value());
    ASSERT_EQ(ack.value().header.messageType, std::to_underlying(MessageType::ORDER_ACK));
    ASSERT_EQ(ack.value().payload.acceptedPrice, 200);

    ASSERT_TRUE(api->getBestBid().has_value());
    ASSERT_EQ(api->getBestBid().value(), 200);
    logout();
}

TEST_F(ProtocolHandlingTests, InvalidHMACNewOrder) {
    login();
    std::fill(client->getSession().hmacKey.begin(), client->getSession().hmacKey.end(),
              0x00);
    client->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(serverFD);
    ASSERT_EQ(serverCapture.size(), 0);
    std::optional<Message<server::OrderAckPayload>> ack =
        deserializeMessage<server::OrderAckPayload>(serverCapture);

    ASSERT_FALSE(ack.has_value());
}

TEST_F(ProtocolHandlingTests, SubmitOrderWithInvalidPrice) {
    login();
    client->sendMessage(testOrderMessage(100, 0, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(serverFD);

    ASSERT_EQ(serverCapture.size(),
              server::PayloadTraits<server::OrderAckPayload>::msgSize);

    std::optional<Message<server::OrderAckPayload>> ack =
        deserializeMessage<server::OrderAckPayload>(serverCapture);

    ASSERT_TRUE(ack.has_value());
    ASSERT_EQ(ack.value().header.messageType, std::to_underlying(MessageType::ORDER_ACK));
    ASSERT_EQ(ack.value().payload.status,
              std::to_underlying(statusCodes::OrderAckStatus::INVALID));

    ASSERT_FALSE(api->getBestBid().has_value());
    logout();
}

TEST_F(ProtocolHandlingTests, SubmitOrderWithInvalidQty) {
    login();
    client->sendMessage(testOrderMessage(0, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(serverFD);

    ASSERT_EQ(serverCapture.size(),
              server::PayloadTraits<server::OrderAckPayload>::msgSize);

    std::optional<Message<server::OrderAckPayload>> ack =
        deserializeMessage<server::OrderAckPayload>(serverCapture);

    ASSERT_TRUE(ack.has_value());
    ASSERT_EQ(ack.value().header.messageType, std::to_underlying(MessageType::ORDER_ACK));
    ASSERT_EQ(ack.value().payload.status,
              std::to_underlying(statusCodes::OrderAckStatus::INVALID));

    ASSERT_FALSE(api->getBestBid().has_value());
    logout();
}

TEST_F(ProtocolHandlingTests, SubmitOrderWhenNotAuthenticated) {
    client->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(serverFD);
    ASSERT_EQ(serverCapture.size(), 0);
    std::optional<Message<server::OrderAckPayload>> ack =
        deserializeMessage<server::OrderAckPayload>(serverCapture);

    ASSERT_FALSE(ack.has_value());
}

TEST_F(ProtocolHandlingTests, CancelOrder) {
    login();
    client->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(serverFD);
    std::optional<Message<server::OrderAckPayload>> ack =
        deserializeMessage<server::OrderAckPayload>(serverCapture);
    ASSERT_TRUE(ack.has_value());
    ASSERT_EQ(ack.value().payload.status,
              std::to_underlying(statusCodes::OrderAckStatus::ACCEPTED));
    clearSendBuffers();
    resetServerCapture();
    resetClientCapture();

    client->sendCancel(ack.value().payload.serverOrderID);
    handler->onMessage(serverFD);

    ASSERT_EQ(serverCapture.size(),
              server::PayloadTraits<server::CancelAckPayload>::msgSize);

    std::optional<Message<server::CancelAckPayload>> cancelAck =
        deserializeMessage<server::CancelAckPayload>(serverCapture);

    ASSERT_TRUE(cancelAck.has_value());
    ASSERT_EQ(cancelAck.value().header.messageType,
              std::to_underlying(MessageType::CANCEL_ACK));
    ASSERT_EQ(cancelAck.value().payload.status,
              std::to_underlying(statusCodes::CancelAckStatus::ACCEPTED));

    ASSERT_FALSE(api->getBestBid().has_value());
    logout();
}

TEST_F(ProtocolHandlingTests, CancelOrderWhenNotAuthenticated) {
    client->sendCancel(1);
    handler->onMessage(serverFD);
    ASSERT_EQ(serverCapture.size(), 0);
    std::optional<Message<server::CancelAckPayload>> cancelAck =
        deserializeMessage<server::CancelAckPayload>(serverCapture);

    ASSERT_FALSE(cancelAck.has_value());
}

TEST_F(ProtocolHandlingTests, CancelOrderNotFound) {
    login();
    client->sendCancel(1);
    handler->onMessage(serverFD);

    ASSERT_EQ(serverCapture.size(),
              server::PayloadTraits<server::CancelAckPayload>::msgSize);

    std::optional<Message<server::CancelAckPayload>> cancelAck =
        deserializeMessage<server::CancelAckPayload>(serverCapture);

    ASSERT_TRUE(cancelAck.has_value());
    ASSERT_EQ(cancelAck.value().header.messageType,
              std::to_underlying(MessageType::CANCEL_ACK));
    ASSERT_EQ(cancelAck.value().payload.status,
              std::to_underlying(statusCodes::CancelAckStatus::NOT_FOUND));

    logout();
}

TEST_F(ProtocolHandlingTests, CancelOrderWithInvalidHMAC) {
    login();
    client->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(serverFD);
    resetServerCapture();
    std::fill(client->getSession().hmacKey.begin(), client->getSession().hmacKey.end(),
              0x00);
    client->sendCancel(1);
    handler->onMessage(serverFD);
    ASSERT_EQ(serverCapture.size(), 0);
    std::optional<Message<server::CancelAckPayload>> cancelAck =
        deserializeMessage<server::CancelAckPayload>(serverCapture);

    ASSERT_FALSE(cancelAck.has_value());
}

TEST_F(ProtocolHandlingTests, ModifyInPlace) {
    login();
    client->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(serverFD);
    std::optional<Message<server::OrderAckPayload>> ack =
        deserializeMessage<server::OrderAckPayload>(serverCapture);
    ASSERT_TRUE(ack.has_value());
    ASSERT_EQ(ack.value().payload.status,
              std::to_underlying(statusCodes::OrderAckStatus::ACCEPTED));
    clearSendBuffers();
    resetServerCapture();
    resetClientCapture();

    client->sendModify(ack.value().payload.serverOrderID, 99, 200);
    handler->onMessage(serverFD);

    ASSERT_EQ(serverCapture.size(),
              server::PayloadTraits<server::ModifyAckPayload>::msgSize);

    std::optional<Message<server::ModifyAckPayload>> modifyAck =
        deserializeMessage<server::ModifyAckPayload>(serverCapture);

    ASSERT_TRUE(modifyAck.has_value());
    ASSERT_EQ(modifyAck.value().header.messageType,
              std::to_underlying(MessageType::MODIFY_ACK));
    ASSERT_EQ(modifyAck.value().payload.status,
              std::to_underlying(statusCodes::ModifyStatus::ACCEPTED));

    ASSERT_TRUE(api->getBestBid().has_value());
    ASSERT_EQ(api->getBestBid().value(), 200);
    ASSERT_EQ(ack.value().payload.serverOrderID,
              modifyAck.value().payload.oldServerOrderID);
    logout();
}

TEST_F(ProtocolHandlingTests, ModifyOrderWhenNotAuthenticated) {
    client->sendModify(1, 99, 200);
    handler->onMessage(serverFD);
    ASSERT_EQ(serverCapture.size(), 0);
    std::optional<Message<server::ModifyAckPayload>> modifyAck =
        deserializeMessage<server::ModifyAckPayload>(serverCapture);

    ASSERT_FALSE(modifyAck.has_value());
}

TEST_F(ProtocolHandlingTests, ModifyOrderNotFound) {
    login();
    client->sendModify(1, 99, 200);
    handler->onMessage(serverFD);

    ASSERT_EQ(serverCapture.size(),
              server::PayloadTraits<server::ModifyAckPayload>::msgSize);

    std::optional<Message<server::ModifyAckPayload>> modifyAck =
        deserializeMessage<server::ModifyAckPayload>(serverCapture);

    ASSERT_TRUE(modifyAck.has_value());
    ASSERT_EQ(modifyAck.value().header.messageType,
              std::to_underlying(MessageType::MODIFY_ACK));
    ASSERT_EQ(modifyAck.value().payload.status,
              std::to_underlying(statusCodes::ModifyStatus::NOT_FOUND));

    logout();
}

TEST_F(ProtocolHandlingTests, ModifyOrderWithInvalidHMAC) {
    login();
    client->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(serverFD);
    resetServerCapture();
    std::fill(client->getSession().hmacKey.begin(), client->getSession().hmacKey.end(),
              0x00);
    client->sendModify(1, 99, 200);
    handler->onMessage(serverFD);
    ASSERT_EQ(serverCapture.size(), 0);
    std::optional<Message<server::ModifyAckPayload>> modifyAck =
        deserializeMessage<server::ModifyAckPayload>(serverCapture);

    ASSERT_FALSE(modifyAck.has_value());
}

TEST_F(ProtocolHandlingTests, ModifyPrice) {
    login();
    client->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(serverFD);
    std::optional<Message<server::OrderAckPayload>> ack =
        deserializeMessage<server::OrderAckPayload>(serverCapture);
    ASSERT_TRUE(ack.has_value());
    ASSERT_EQ(ack.value().payload.status,
              std::to_underlying(statusCodes::OrderAckStatus::ACCEPTED));
    clearSendBuffers();
    resetServerCapture();
    resetClientCapture();

    client->sendModify(ack.value().payload.serverOrderID, 100, 250);
    handler->onMessage(serverFD);

    ASSERT_EQ(serverCapture.size(),
              server::PayloadTraits<server::ModifyAckPayload>::msgSize);

    std::optional<Message<server::ModifyAckPayload>> modifyAck =
        deserializeMessage<server::ModifyAckPayload>(serverCapture);

    ASSERT_TRUE(modifyAck.has_value());
    ASSERT_EQ(modifyAck.value().header.messageType,
              std::to_underlying(MessageType::MODIFY_ACK));
    ASSERT_EQ(modifyAck.value().payload.status,
              std::to_underlying(statusCodes::ModifyStatus::ACCEPTED));

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
    client->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(serverFD);
    std::optional<Message<server::OrderAckPayload>> ack =
        deserializeMessage<server::OrderAckPayload>(serverCapture);
    ASSERT_TRUE(ack.has_value());
    ASSERT_EQ(ack.value().payload.status,
              std::to_underlying(statusCodes::OrderAckStatus::ACCEPTED));
    clearSendBuffers();
    resetServerCapture();
    resetClientCapture();

    client->sendModify(ack.value().payload.serverOrderID, 50, 250);
    handler->onMessage(serverFD);

    ASSERT_EQ(serverCapture.size(),
              server::PayloadTraits<server::ModifyAckPayload>::msgSize);

    std::optional<Message<server::ModifyAckPayload>> modifyAck =
        deserializeMessage<server::ModifyAckPayload>(serverCapture);

    ASSERT_TRUE(modifyAck.has_value());
    ASSERT_EQ(modifyAck.value().header.messageType,
              std::to_underlying(MessageType::MODIFY_ACK));
    ASSERT_EQ(modifyAck.value().payload.status,
              std::to_underlying(statusCodes::ModifyStatus::ACCEPTED));

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
    client->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(serverFD);
    std::optional<Message<server::OrderAckPayload>> ack =
        deserializeMessage<server::OrderAckPayload>(serverCapture);
    ASSERT_TRUE(ack.has_value());
    ASSERT_EQ(ack.value().payload.status,
              std::to_underlying(statusCodes::OrderAckStatus::ACCEPTED));
    clearSendBuffers();
    resetServerCapture();
    resetClientCapture();

    client->sendMessage(testOrderMessage(100, 201, OrderSide::SELL, OrderType::LIMIT));
    handler->onMessage(serverFD);
    std::optional<Message<server::OrderAckPayload>> sellAck =
        deserializeMessage<server::OrderAckPayload>(serverCapture);
    ASSERT_TRUE(sellAck.has_value());
    ASSERT_EQ(sellAck.value().payload.status,
              std::to_underlying(statusCodes::OrderAckStatus::ACCEPTED));
    clearSendBuffers();
    resetServerCapture();
    resetClientCapture();

    client->sendModify(ack.value().payload.serverOrderID, 100, 201);
    handler->onMessage(serverFD);
    ASSERT_EQ(serverCapture.size(),
              server::PayloadTraits<server::ModifyAckPayload>::msgSize +
                  server::PayloadTraits<server::TradePayload>::msgSize * 2);

    auto modifyAck = deserializeMessage<server::ModifyAckPayload>(
        std::span(serverCapture)
            .subspan(0, server::PayloadTraits<server::ModifyAckPayload>::msgSize));
    ASSERT_TRUE(modifyAck.has_value());
    ASSERT_EQ(modifyAck.value().header.messageType,
              std::to_underlying(MessageType::MODIFY_ACK));
    ASSERT_EQ(modifyAck.value().payload.status,
              std::to_underlying(statusCodes::ModifyStatus::ACCEPTED));
    auto trade1 = deserializeMessage<server::TradePayload>(
        std::span(serverCapture)
            .subspan(server::PayloadTraits<server::ModifyAckPayload>::msgSize));
    ASSERT_TRUE(trade1.has_value());
    ASSERT_EQ(trade1.value().header.messageType, std::to_underlying(MessageType::TRADE));
    ASSERT_EQ(trade1.value().payload.filledQty, 100);
    ASSERT_EQ(trade1.value().payload.filledPrice, 201);

    auto trade2 = deserializeMessage<server::TradePayload>(
        std::span(serverCapture)
            .subspan(server::PayloadTraits<server::ModifyAckPayload>::msgSize +
                     server::PayloadTraits<server::TradePayload>::msgSize));
    ASSERT_TRUE(trade2.has_value());
    ASSERT_EQ(trade2.value().header.messageType, std::to_underlying(MessageType::TRADE));
    ASSERT_EQ(trade2.value().payload.filledQty, 100);
    ASSERT_EQ(trade2.value().payload.filledPrice, 201);
}

TEST_F(ProtocolHandlingTests, MultipleOrders) {
    login();
    for (int i = 0; i < 10; ++i) {
        client->sendMessage(
            testOrderMessage(100 + i, 200 + i, OrderSide::BUY, OrderType::LIMIT));
        handler->onMessage(serverFD);
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

    auto hmac = computeHMAC_(client->getSession().hmacKey, serialized.data(),
                             client::PayloadTraits<client::NewOrderPayload>::dataSize);

    std::copy(hmac.begin(), hmac.end(),
              serialized.data() +
                  client::PayloadTraits<client::NewOrderPayload>::hmacOffset);

    size_t half = serialized.size() / 2;

    serverSession->recvBuffer.insert(serverSession->recvBuffer.end(), serialized.begin(),
                                     serialized.begin() + half);
    handler->onMessage(serverFD);
    ASSERT_TRUE(serverCapture.empty());

    serverSession->recvBuffer.insert(serverSession->recvBuffer.end(),
                                     serialized.begin() + half, serialized.end());
    handler->onMessage(serverFD);
    ASSERT_FALSE(serverCapture.empty());

    auto ack = deserializeMessage<server::OrderAckPayload>(serverCapture);
    ASSERT_TRUE(ack.has_value());
    ASSERT_EQ(ack.value().header.messageType, std::to_underlying(MessageType::ORDER_ACK));
}
