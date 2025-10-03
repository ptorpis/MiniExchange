#include "client/client.hpp"
#include "client/clientNetwork.hpp"
#include "protocol/client/clientMessageFactory.hpp"
#include "protocol/client/clientMessages.hpp"
#include "protocol/protocolHandler.hpp"
#include "protocol/traits.hpp"
#include "utils/orderBookRenderer.hpp"
#include "utils/types.hpp"

#include <gtest/gtest.h>
#include <memory>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <stdexcept>
#include <vector>

class ProtocolTests : public ::testing::Test {
protected:
    std::vector<uint8_t> computeHMAC_(const std::array<uint8_t, 32>& key,
                                      const uint8_t* data, size_t dataLen) {
        unsigned int len = 32;
        std::vector<uint8_t> hmac(len);
        HMAC(EVP_sha256(), key.data(), key.size(), data, dataLen, hmac.data(), &len);
        return hmac;
    }

    void SetUp() override {
        utils::OrderBookRenderer::enabled = false;
        sessionManager = std::make_unique<SessionManager>();
        handler = std::make_unique<ProtocolHandler>(
            *sessionManager.get(), [this]([[maybe_unused]] Session& session,
                                          const std::span<const uint8_t> buffer) {
                serverCapture.insert(serverCapture.end(), buffer.begin(), buffer.end());
            });
        std::fill(apiKey.begin(), apiKey.end(), 0x22);
        std::fill(hmacKey.begin(), hmacKey.end(), 0x11);

        buyer = std::make_unique<Client>(
            hmacKey, apiKey, [this](const std::span<const uint8_t> buffer) {
                clientCapture.insert(clientCapture.end(), buffer.begin(), buffer.end());
                serverBuyerSession->recvBuffer.insert(
                    serverBuyerSession->recvBuffer.end(), buffer.begin(), buffer.end());
            });
        seller = std::make_unique<Client>(
            hmacKey, apiKey, [this](const std::span<const uint8_t> buffer) {
                clientCapture.insert(clientCapture.end(), buffer.begin(), buffer.end());
                serverSellerSession->recvBuffer.insert(
                    serverSellerSession->recvBuffer.end(), buffer.begin(), buffer.end());
            });

        buyerSession = &buyer->getSession();
        sellerSession = &seller->getSession();

        api = handler->getAPI();
        if (!api) {
            throw std::runtime_error("API is null");
        }

        serverBuyerSession = &handler->createSession(buyerFD);
        serverSellerSession = &handler->createSession(sellerFD);
    }

    void insertIntoRecvBuffer(ClientSession& session, const std::vector<uint8_t>& data) {
        session.recvBuffer.insert(session.recvBuffer.end(), data.begin(), data.end());
    }

    Message<client::NewOrderPayload> testOrderMessage(ClientSession& session, Qty qty,
                                                      Price price, OrderSide side,
                                                      OrderType type) {
        Message<client::NewOrderPayload> msg;
        msg.header = client::makeClientHeader<client::NewOrderPayload>(session);
        msg.payload.serverClientID = session.serverClientID;
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

    void resetCapture() { clientCapture.clear(); }

    void login(bool initial = true) {
        if (initial) {

            EXPECT_FALSE(buyer->getAuthStatus());
            EXPECT_FALSE(seller->getAuthStatus());

            EXPECT_FALSE(serverBuyerSession->authenticated);
            EXPECT_FALSE(serverSellerSession->authenticated);
        }

        buyer->sendHello();
        seller->sendHello();

        handler->onMessage(buyerFD);
        insertIntoRecvBuffer(*buyerSession, serverCapture);
        buyer->processIncoming();
        resetServerCapture();

        handler->onMessage(sellerFD);
        insertIntoRecvBuffer(*sellerSession, serverCapture);
        seller->processIncoming();
        resetServerCapture();
        buyer->clearSendBuffer();
        seller->clearSendBuffer();

        EXPECT_TRUE(buyer->getAuthStatus());
        EXPECT_TRUE(seller->getAuthStatus());
        EXPECT_TRUE(serverBuyerSession->authenticated);
        EXPECT_TRUE(serverSellerSession->authenticated);
        clearSendBuffers();
        resetClientCapture();
    }

    void logout() {
        EXPECT_TRUE(buyer->getAuthStatus());
        EXPECT_TRUE(seller->getAuthStatus());

        EXPECT_TRUE(serverBuyerSession->authenticated);
        EXPECT_TRUE(serverSellerSession->authenticated);

        buyer->sendLogout();
        seller->sendLogout();

        handler->onMessage(buyerFD);
        insertIntoRecvBuffer(*buyerSession, serverCapture);
        buyer->processIncoming();
        resetServerCapture();

        handler->onMessage(sellerFD);
        insertIntoRecvBuffer(*sellerSession, serverCapture);
        seller->processIncoming();
        resetServerCapture();
        buyer->clearSendBuffer();
        seller->clearSendBuffer();

        EXPECT_FALSE(buyer->getAuthStatus());
        EXPECT_FALSE(seller->getAuthStatus());

        EXPECT_FALSE(serverBuyerSession->authenticated);
        EXPECT_FALSE(serverSellerSession->authenticated);
        clearSendBuffers();
        resetClientCapture();
    }

    void resetServerCapture() { serverCapture.clear(); }
    void resetClientCapture() { clientCapture.clear(); }

    void clearSendBuffers() {
        buyer->clearSendBuffer();
        buyerSession->sendBuffer.clear();
        seller->clearSendBuffer();
        sellerSession->sendBuffer.clear();
    }

private:
    std::unique_ptr<SessionManager> sessionManager;
    std::unique_ptr<ProtocolHandler> handler;

    ApiKey apiKey;
    HMACKey hmacKey;

    std::unique_ptr<Client> buyer;
    std::unique_ptr<Client> seller;
    MiniExchangeAPI* api;

    ClientSession* buyerSession;
    ClientSession* sellerSession;

    Session* serverBuyerSession;
    Session* serverSellerSession;

    int buyerFD = 1;
    int sellerFD = 2;

    std::vector<uint8_t> clientCapture;
    std::vector<uint8_t> serverCapture;
};

TEST_F(ProtocolTests, hello) {
    login();
    logout();
}