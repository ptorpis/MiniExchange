#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <list>
#include <optional>
#include <span>
#include <vector>

#include "auth/session.hpp"
#include "protocol/client/clientMessages.hpp"
#include "protocol/messages.hpp"
#include "protocol/serialize.hpp"

class Client {
public:
    using SendFn = std::function<void(const std::span<const uint8_t>)>;

    Client(std::array<uint8_t, 16> APIKey, SendFn sendFn = {})
        : APIKey_(APIKey), session_(), sendFn_(std::move(sendFn)) {
        session_.reserve();
        if (!sendFn_) {
            sendFn_ = [this](const std::span<const uint8_t> buffer) {
                session_.sendBuffer.insert(session_.sendBuffer.end(), buffer.begin(),
                                           buffer.end());
            };
        }
    }

    ClientSession& getSession() { return session_; }

    std::list<client::IncomingMessageVariant> processIncoming();

    std::array<uint8_t, 16> getAPIKey() { return APIKey_; }
    const std::vector<uint8_t>& readRecBuffer() const { return session_.recvBuffer; }
    const std::vector<uint8_t>& readSendBuffer() const { return session_.sendBuffer; }

    template <typename Payload> void sendMessage(Message<Payload> msg);

    void sendHello();
    void sendLogout();
    void sendOrder(Qty qty, Price price, bool isBuy, bool isLimit);

    void sendCancel(OrderID orderID);
    void cancelAll();
    void sendModify(OrderID orderID, Qty newQty, Price newPrice);

    void sendHeartbeat();

    void appendRecvBuffer(std::span<const uint8_t> data);

    bool getAuthStatus() { return session_.authenticated; }

    void clearSendBuffer() { session_.sendBuffer.clear(); }
    void clearRecvBuffer() { session_.recvBuffer.clear(); }

    void stop() {
        std::cout << "Client stopped\n";
        running_ = false;
    }
    bool isRunning() const { return running_; }

    void addOutstandingOrder(OrderID orderID, Qty qty, Price price);
    void removeOutstandingOrder(OrderID orderID);
    void modifyOutstandingOrder(OrderID orderID, OrderID newOrderID, Qty newQty,
                                Price newPrice);
    void fillOutstandingOrder(OrderID orderID, Qty filledQty);

    const std::unordered_map<OrderID, OutstandingOrder>& getOutstandingOrders() const {
        return outstandingOrders_;
    }

private:
    std::array<uint8_t, 16> APIKey_;

    std::optional<MessageHeader> peekHeader_() const;
    void eraseBytesFromBuffer(std::vector<uint8_t>& buffer, size_t n_bytes);

    template <typename Payload> std::optional<client::IncomingMessageVariant>
    makeIncomingVariant_(const Message<Payload>& msg, size_t n_bytes) {
        eraseBytesFromBuffer(session_.recvBuffer, n_bytes);
        return client::IncomingMessageVariant{msg.payload};
    }

    std::optional<client::IncomingMessageVariant> processIncomingMessage_();

    ClientSession session_;
    SendFn sendFn_;
    bool running_{true};

    std::unordered_map<OrderID, OutstandingOrder> outstandingOrders_;
};

template <typename Payload> void Client::sendMessage(Message<Payload> msg) {
    std::vector<uint8_t> serialized = serializeMessage<Payload>(
        client::PayloadTraits<Payload>::type, msg.payload, msg.header);
    sendFn_(serialized);
}