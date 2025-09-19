#include "api/api.hpp"
#include "auth/sessionManager.hpp"
#include "client/client.hpp"
#include "core/service.hpp"
#include "network/networkHandler.hpp"
#include <gtest/gtest.h>
#include <memory>

class NetworkHandlerTest : public ::testing::Test {
protected:
    // HMAC KEY is 0x11 repeating
    // API KEY is 0x22 repeating

    // After the SetUp(), the client is ready to log in, but the login()
    // method has been moved into the test cases

    void SetUp() override {
        serverFD = 1;
        clientFD = 2;
        std::fill(HMACKEY.begin(), HMACKEY.end(), 0x11);
        std::fill(APIKEY.begin(), APIKEY.end(), 0x22);
        api = std::make_unique<MiniExchangeAPI>(engine, sessionManager, service);
        serverSession = &sessionManager.createSession(serverFD);
        client = std::make_unique<Client>(
            HMACKEY, APIKEY, clientFD, [this](const std::span<const uint8_t> buffer) {
                clientCapture.insert(clientCapture.end(), buffer.begin(), buffer.end());
                if (serverSession) {
                    serverSession->recvBuffer.insert(serverSession->recvBuffer.end(),
                                                     buffer.begin(), buffer.end());
                }
            });

        handler = std::make_unique<NetworkHandler>(
            *api, sessionManager,
            [this]([[maybe_unused]] Session& session, std::span<const uint8_t> buffer) {
                serverCapture.insert(serverCapture.end(), buffer.begin(), buffer.end());

                if (client) {
                    client->appendRecvBuffer(buffer);
                }
            });

        serverSession->hmacKey = HMACKEY;
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

    void appendServerRecvBuffer(std::span<const uint8_t> buffer) {
        serverSession->recvBuffer.insert(serverSession->recvBuffer.end(), buffer.begin(),
                                         buffer.end());
    }

    void resetServerCapture() { serverCapture.clear(); }
    void resetClientCapture() { clientCapture.clear(); }

    void clearSendBuffers() {
        client->clearSendBuffer();
        serverSession->sendBuffer.clear();
    }

    Message<NewOrderPayload> testOrderMessage(Qty qty, Price price, OrderSide side,
                                              OrderType type) {
        Message<NewOrderPayload> msg;
        msg.header = makeClientHeader<NewOrderPayload>(client->getSession());
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

    MatchingEngine engine;
    SessionManager sessionManager;
    OrderService service;
    std::unique_ptr<MiniExchangeAPI> api;
    std::unique_ptr<NetworkHandler> handler;
    Session* serverSession;
    std::unique_ptr<Client> client;
    std::vector<uint8_t> serverCapture; // data from the server send buffer
    std::vector<uint8_t> clientCapture; // data from the clinet send buffer
    std::array<uint8_t, 32> HMACKEY;
    std::array<uint8_t, 16> APIKEY;

    int serverFD;
    int clientFD;
};

TEST_F(NetworkHandlerTest, BaseCase) {
    login();
    ASSERT_TRUE(serverSession->authenticated);
    logout();
}

/*
When a client tries to log in more than one time, and they are already
authenticated, the server should ignore the second and subsequent HELLO
messages.
*/
TEST_F(NetworkHandlerTest, DoubleLogin) {
    login();
    login(/* is initial */ false);
    ASSERT_EQ(serverCapture.size(), 0);

    logout();
}

TEST_F(NetworkHandlerTest, LogoutWhenNotAuthenticated) {
    logout();
    ASSERT_EQ(serverCapture.size(), 0);
}

TEST_F(NetworkHandlerTest, SubmitOrder) {
    login();
    client->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(serverFD);

    ASSERT_EQ(serverCapture.size(), PayloadTraits<OrderAckPayload>::msgSize);

    std::optional<Message<OrderAckPayload>> ack =
        deserializeMessage<OrderAckPayload>(serverCapture);

    ASSERT_TRUE(ack.has_value());
    ASSERT_EQ(ack.value().header.messageType, std::to_underlying(MessageType::ORDER_ACK));
    ASSERT_EQ(ack.value().payload.acceptedPrice, 200);

    ASSERT_TRUE(engine.getBestBid().has_value());
    ASSERT_EQ(engine.getBestBid().value(), 200);
    logout();
}

TEST_F(NetworkHandlerTest, InvalidHMACNewOrder) {
    login();
    std::fill(client->getSession().hmacKey.begin(), client->getSession().hmacKey.end(),
              0x00);
    client->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(serverFD);
    ASSERT_EQ(serverCapture.size(), 0);
    std::optional<Message<OrderAckPayload>> ack =
        deserializeMessage<OrderAckPayload>(serverCapture);

    ASSERT_FALSE(ack.has_value());
}

TEST_F(NetworkHandlerTest, SubmitOrderWithInvalidPrice) {
    login();
    client->sendMessage(testOrderMessage(100, 0, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(serverFD);

    ASSERT_EQ(serverCapture.size(), PayloadTraits<OrderAckPayload>::msgSize);

    std::optional<Message<OrderAckPayload>> ack =
        deserializeMessage<OrderAckPayload>(serverCapture);

    ASSERT_TRUE(ack.has_value());
    ASSERT_EQ(ack.value().header.messageType, std::to_underlying(MessageType::ORDER_ACK));
    ASSERT_EQ(ack.value().payload.status,
              std::to_underlying(statusCodes::OrderAckStatus::INVALID));

    ASSERT_FALSE(engine.getBestBid().has_value());
    logout();
}

TEST_F(NetworkHandlerTest, SubmitOrderWithInvalidQty) {
    login();
    client->sendMessage(testOrderMessage(0, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(serverFD);

    ASSERT_EQ(serverCapture.size(), PayloadTraits<OrderAckPayload>::msgSize);

    std::optional<Message<OrderAckPayload>> ack =
        deserializeMessage<OrderAckPayload>(serverCapture);

    ASSERT_TRUE(ack.has_value());
    ASSERT_EQ(ack.value().header.messageType, std::to_underlying(MessageType::ORDER_ACK));
    ASSERT_EQ(ack.value().payload.status,
              std::to_underlying(statusCodes::OrderAckStatus::INVALID));

    ASSERT_FALSE(engine.getBestBid().has_value());
    logout();
}

TEST_F(NetworkHandlerTest, SubmitOrderWhenNotAuthenticated) {
    client->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(serverFD);
    ASSERT_EQ(serverCapture.size(), 0);
    std::optional<Message<OrderAckPayload>> ack =
        deserializeMessage<OrderAckPayload>(serverCapture);

    ASSERT_FALSE(ack.has_value());
}

TEST_F(NetworkHandlerTest, CancelOrder) {
    login();
    client->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(serverFD);
    std::optional<Message<OrderAckPayload>> ack =
        deserializeMessage<OrderAckPayload>(serverCapture);
    ASSERT_TRUE(ack.has_value());
    ASSERT_EQ(ack.value().payload.status,
              std::to_underlying(statusCodes::OrderAckStatus::ACCEPTED));
    clearSendBuffers();
    resetServerCapture();
    resetClientCapture();

    client->sendCancel(ack.value().payload.serverOrderID);
    handler->onMessage(serverFD);

    ASSERT_EQ(serverCapture.size(), PayloadTraits<CancelAckPayload>::msgSize);

    std::optional<Message<CancelAckPayload>> cancelAck =
        deserializeMessage<CancelAckPayload>(serverCapture);

    ASSERT_TRUE(cancelAck.has_value());
    ASSERT_EQ(cancelAck.value().header.messageType,
              std::to_underlying(MessageType::CANCEL_ACK));
    ASSERT_EQ(cancelAck.value().payload.status,
              std::to_underlying(statusCodes::CancelAckStatus::ACCEPTED));

    ASSERT_FALSE(engine.getBestBid().has_value());
    logout();
}

TEST_F(NetworkHandlerTest, CancelOrderWhenNotAuthenticated) {
    client->sendCancel(1);
    handler->onMessage(serverFD);
    ASSERT_EQ(serverCapture.size(), 0);
    std::optional<Message<CancelAckPayload>> cancelAck =
        deserializeMessage<CancelAckPayload>(serverCapture);

    ASSERT_FALSE(cancelAck.has_value());
}

TEST_F(NetworkHandlerTest, CancelOrderNotFound) {
    login();
    client->sendCancel(1);
    handler->onMessage(serverFD);

    ASSERT_EQ(serverCapture.size(), PayloadTraits<CancelAckPayload>::msgSize);

    std::optional<Message<CancelAckPayload>> cancelAck =
        deserializeMessage<CancelAckPayload>(serverCapture);

    ASSERT_TRUE(cancelAck.has_value());
    ASSERT_EQ(cancelAck.value().header.messageType,
              std::to_underlying(MessageType::CANCEL_ACK));
    ASSERT_EQ(cancelAck.value().payload.status,
              std::to_underlying(statusCodes::CancelAckStatus::NOT_FOUND));

    logout();
}

TEST_F(NetworkHandlerTest, CancelOrderWithInvalidHMAC) {
    login();
    client->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(serverFD);
    resetServerCapture();
    std::fill(client->getSession().hmacKey.begin(), client->getSession().hmacKey.end(),
              0x00);
    client->sendCancel(1);
    handler->onMessage(serverFD);
    ASSERT_EQ(serverCapture.size(), 0);
    std::optional<Message<CancelAckPayload>> cancelAck =
        deserializeMessage<CancelAckPayload>(serverCapture);

    ASSERT_FALSE(cancelAck.has_value());
}

TEST_F(NetworkHandlerTest, ModifyInPlace) {
    login();
    client->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(serverFD);
    std::optional<Message<OrderAckPayload>> ack =
        deserializeMessage<OrderAckPayload>(serverCapture);
    ASSERT_TRUE(ack.has_value());
    ASSERT_EQ(ack.value().payload.status,
              std::to_underlying(statusCodes::OrderAckStatus::ACCEPTED));
    clearSendBuffers();
    resetServerCapture();
    resetClientCapture();

    client->sendModify(ack.value().payload.serverOrderID, 99, 200);
    handler->onMessage(serverFD);

    ASSERT_EQ(serverCapture.size(), PayloadTraits<ModifyAckPayload>::msgSize);

    std::optional<Message<ModifyAckPayload>> modifyAck =
        deserializeMessage<ModifyAckPayload>(serverCapture);

    ASSERT_TRUE(modifyAck.has_value());
    ASSERT_EQ(modifyAck.value().header.messageType,
              std::to_underlying(MessageType::MODIFY_ACK));
    ASSERT_EQ(modifyAck.value().payload.status,
              std::to_underlying(statusCodes::ModifyStatus::ACCEPTED));

    ASSERT_TRUE(engine.getBestBid().has_value());
    ASSERT_EQ(engine.getBestBid().value(), 200);
    ASSERT_EQ(ack.value().payload.serverOrderID,
              modifyAck.value().payload.oldServerOrderID);
    logout();
}

TEST_F(NetworkHandlerTest, ModifyOrderWhenNotAuthenticated) {
    client->sendModify(1, 99, 200);
    handler->onMessage(serverFD);
    ASSERT_EQ(serverCapture.size(), 0);
    std::optional<Message<ModifyAckPayload>> modifyAck =
        deserializeMessage<ModifyAckPayload>(serverCapture);

    ASSERT_FALSE(modifyAck.has_value());
}

TEST_F(NetworkHandlerTest, ModifyOrderNotFound) {
    login();
    client->sendModify(1, 99, 200);
    handler->onMessage(serverFD);

    ASSERT_EQ(serverCapture.size(), PayloadTraits<ModifyAckPayload>::msgSize);

    std::optional<Message<ModifyAckPayload>> modifyAck =
        deserializeMessage<ModifyAckPayload>(serverCapture);

    ASSERT_TRUE(modifyAck.has_value());
    ASSERT_EQ(modifyAck.value().header.messageType,
              std::to_underlying(MessageType::MODIFY_ACK));
    ASSERT_EQ(modifyAck.value().payload.status,
              std::to_underlying(statusCodes::ModifyStatus::NOT_FOUND));

    logout();
}

TEST_F(NetworkHandlerTest, ModifyOrderWithInvalidHMAC) {
    login();
    client->sendMessage(testOrderMessage(100, 200, OrderSide::BUY, OrderType::LIMIT));
    handler->onMessage(serverFD);
    resetServerCapture();
    std::fill(client->getSession().hmacKey.begin(), client->getSession().hmacKey.end(),
              0x00);
    client->sendModify(1, 99, 200);
    handler->onMessage(serverFD);
    ASSERT_EQ(serverCapture.size(), 0);
    std::optional<Message<ModifyAckPayload>> modifyAck =
        deserializeMessage<ModifyAckPayload>(serverCapture);

    ASSERT_FALSE(modifyAck.has_value());
}

TEST_F(NetworkHandlerTest, MultipleOrders) {
    login();
    for (int i = 0; i < 10; ++i) {
        client->sendMessage(
            testOrderMessage(100 + i, 200 + i, OrderSide::BUY, OrderType::LIMIT));
        handler->onMessage(serverFD);
        resetServerCapture();
    }

    ASSERT_TRUE(engine.getBestBid().has_value());
    ASSERT_EQ(engine.getBestBid().value(), 209);
    ASSERT_EQ(engine.getBidsSize(), 10);
    logout();
}
