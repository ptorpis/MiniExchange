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

    // At the beginning of each test case, we can assume that the user is logged in

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

        login();
    }

    void login() {
        resetClientCapture();
        resetServerCapture();
        EXPECT_FALSE(client->getAuthStatus());
        EXPECT_FALSE(serverSession->authenticated);
        client->sendHello();

        handler->onMessage(serverFD);
        client->clearSendBuffer();

        client->processIncoming();
        serverSession->sendBuffer.clear();

        EXPECT_TRUE(client->getAuthStatus());
        EXPECT_TRUE(serverSession->authenticated);
        clearSendBuffers();
        resetClientCapture();
        resetServerCapture();
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
    ASSERT_TRUE(serverSession->authenticated);
    logout();
}