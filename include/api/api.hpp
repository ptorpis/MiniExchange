#pragma once

#include "auth/sessionManager.hpp"
#include "core/matchingEngine.hpp"
#include "core/order.hpp"
#include "protocol/messages.hpp"
#include "protocol/statusCodes.hpp"
#include <optional>

class MiniExchangeAPI {
public:
    MiniExchangeAPI(MatchingEngine& engine, SessionManager& sm)
        : engine_(engine), sessionManager_(sm) {}

    Session& connectClient(int fd);
    Session* getSession(int fd);
    void disconnectClient(int fd);

    std::vector<uint8_t> handleHello(Session& session,
                                     std::optional<Message<HelloPayload>> msg);

    std::vector<uint8_t> handleLogout(Session& session,
                                      std::optional<Message<LogoutPayload>> msg);

    void newOrder(int fd, const Order& order);
    void cancelOrder(int fd, const OrderID orderID);
    void modifyOrder(int fd);

private:
    MatchingEngine& engine_;
    SessionManager& sessionManager_;
    bool isValidAPIKey_(Session& session, const uint8_t key[16]);
    bool verifyHMAC_(const std::array<uint8_t, 32>& key, const uint8_t* data,
                     size_t dataLen, const uint8_t* expectedHMAC, size_t HMACLen);

    std::vector<uint8_t> computeHMAC_(const std::array<uint8_t, 32>& key,
                                      const uint8_t* data, size_t dataLen);

    template <typename Payload>
    void sendMessage_(Session& sesssion, const Message<Payload>& msg);

    std::vector<uint8_t> makeHelloAck_(Session& session, statusCodes::HelloStatus status);
    std::vector<uint8_t> makeLogoutAck_(Session& session,
                                        statusCodes::LogoutStatus status);
};