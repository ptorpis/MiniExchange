#pragma once

#include "protocol/clientMessages.hpp"
#include "protocol/messages.hpp"
#include "protocol/serverMessages.hpp"
#include "utils/types.hpp"
#include "utils/utils.hpp"
#include <cstdint>
#include <string>
#include <vector>

class NetworkClient {
public:
    NetworkClient(std::string host, std::uint16_t port);
    virtual ~NetworkClient();

    bool connect();
    void disconnect();
    bool isConnected() const { return sockfd_ >= 0; }

    void sendHello();
    void sendLogout();
    void sendNewOrder(const client::NewOrderPayload& payload);
    void sendCancel(const client::CancelOrderPayload& payload);
    void sendModify(const client::ModifyOrderPayload& payload);

    void processMessages();

protected:
    virtual void
    onHelloAck([[maybe_unused]] const Message<server::HelloAckPayload>& msg) {
        utils::printMessage(std::cout, msg);
    }

    virtual void
    onLogoutAck([[maybe_unused]] const Message<server::LogoutAckPayload>& msg) {
        utils::printMessage(std::cout, msg);
    }

    virtual void
    onOrderAck([[maybe_unused]] const Message<server::OrderAckPayload>& msg) {
        utils::printMessage(std::cout, msg);
    }

    virtual void
    onCancelAck([[maybe_unused]] const Message<server::CancelAckPayload>& msg) {
        utils::printMessage(std::cout, msg);
    }

    virtual void
    onModifyAck([[maybe_unused]] const Message<server::ModifyAckPayload>& msg) {
        utils::printMessage(std::cout, msg);
    }

    virtual void onTrade([[maybe_unused]] const Message<server::TradePayload>& msg) {
        utils::printMessage(std::cout, msg);
    }

private:
    template <typename Payload>
    void sendMessage_(MessageType type, const Payload& payload);

    void processRecvBuffer_();
    void handleMessage_(std::span<const std::byte> messageBytes);

    void setNonBlocking_();
    void setTCPNoDelay_();

    int sockfd_;
    std::string host_;
    std::uint16_t port_;

    std::vector<std::byte> recvBuffer_;
    std::vector<std::byte> sendBuffer_;

    ClientSqn32 clientSqn{0};
    ServerSqn32 serverSqn{0};

    ClientID serverClientID{0};
};
