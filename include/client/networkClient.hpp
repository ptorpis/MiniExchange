#pragma once

#include "protocol/messages.hpp"
#include "protocol/serverMessages.hpp"
#include "utils/types.hpp"
#include "utils/utils.hpp"
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
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

    void processMessages();

    void sendNewOrder(InstrumentID instrumentID, OrderSide side, OrderType type, Qty qty,
                      Price price, TimeInForce timeInForce, Timestamp goodTillDate);

    void sendCancel(OrderID orderID, InstrumentID instrumentID);

    void sendModify(OrderID orderID, Qty newQty, Price newPrice,
                    InstrumentID instrumentID);

    void setClientID(ClientID id) { serverClientID_ = id; }

    [[nodiscard]] ClientID getClientID() const noexcept { return serverClientID_; }
    [[nodiscard]] ClientOrderID getCurrentClientOrderID() const noexcept {
        return internalOrderCounter_;
    }

    [[nodiscard]] ClientOrderID getNextClientOrderID() noexcept {
        return ++internalOrderCounter_;
    }

protected:
    virtual void
    onHelloAck([[maybe_unused]] const Message<server::HelloAckPayload>& msg) {
        setClientID(ClientID{msg.payload.serverClientID});
        utils::printMessage(std::cout, msg);
    }

    virtual void
    onLogoutAck([[maybe_unused]] const Message<server::LogoutAckPayload>& msg) {
        setClientID(ClientID{0});
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
    void messageLoop_();
    void startMessageLoop_();
    void stopMessageLoop_();

    template <typename Payload>
    void sendMessage_(MessageType type, const Payload& payload);

    void processRecvBuffer_();
    void handleMessage_(std::span<const std::byte> messageBytes);

    void setNonBlocking_();
    void setTCPNoDelay_();

    std::atomic<bool> running_{false};

    int sockfd_;
    std::string host_;
    std::uint16_t port_;

    std::vector<std::byte> recvBuffer_;
    std::vector<std::byte> sendBuffer_;
    std::mutex sendMutex_;

    ClientSqn32 clientSqn{0};
    ServerSqn32 serverSqn{0};
    ClientID serverClientID_{0};

    ClientOrderID internalOrderCounter_{0};

    std::thread messageThread_;
};
