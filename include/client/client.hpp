#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include "auth/session.hpp"
#include "protocol/messages.hpp"
#include "protocol/serialize.hpp"

class Client {
public:
    using SendFn = std::function<void(const std::span<const uint8_t>)>;

    Client(std::array<uint8_t, constants::HMAC_SIZE> hmacKey,
           std::array<uint8_t, 16> APIKey, SendFn sendFn = {})
        : APIKey_(APIKey), session_(), sendFn_(std::move(sendFn)) {
        session_.hmacKey = hmacKey;
        session_.reserve();
        if (!sendFn_) {
            sendFn_ = [this](const std::span<const uint8_t> buffer) {
                session_.sendBuffer.insert(session_.sendBuffer.end(), buffer.begin(),
                                           buffer.end());
            };
        }
    }

    ClientSession& getSession() { return session_; }

    void processIncoming();

    std::array<uint8_t, 16> getAPIKey() { return APIKey_; }
    const std::vector<uint8_t>& readRecBuffer() const { return session_.recvBuffer; }
    const std::vector<uint8_t>& readSendBuffer() const { return session_.sendBuffer; }

    template <typename Payload> void sendMessage(Message<Payload> msg);

    void sendHello();
    void sendLogout();
    void sendOrder(Qty qty, Price price, bool isBuy, bool isLimit);

    void sendCancel(OrderID orderID);
    void sendModify(OrderID orderID, Qty newQty, Price newPrice);

    void sendHeartbeat();

    void appendRecvBuffer(std::span<const uint8_t> data);

    bool getAuthStatus() { return session_.authenticated; }

    void clearSendBuffer() { session_.sendBuffer.clear(); }
    void clearRecvBuffer() { session_.recvBuffer.clear(); }

    std::vector<uint8_t> computeHMAC_(const std::array<uint8_t, 32>& key,
                                      const uint8_t* data, size_t dataLen);

private:
    std::array<uint8_t, 16> APIKey_;

    bool verifyHMAC_(const std::array<uint8_t, 32>& key, const uint8_t* data,
                     size_t dataLen, const uint8_t* expectedHMAC, size_t HMACLen);

    // std::vector<uint8_t> computeHMAC_(const std::array<uint8_t, 32>& key,
    //                                   const uint8_t* data, size_t dataLen);
    std::optional<MessageHeader> peekHeader_() const;

    ClientSession session_;
    SendFn sendFn_;
};

template <typename Payload> void Client::sendMessage(Message<Payload> msg) {
    std::vector<uint8_t> serialized = serializeMessage<Payload>(
        client::PayloadTraits<Payload>::type, msg.payload, msg.header);
    auto hmac = computeHMAC_(session_.hmacKey, serialized.data(),
                             client::PayloadTraits<Payload>::dataSize);
    std::copy(hmac.begin(), hmac.end(),
              serialized.data() + client::PayloadTraits<Payload>::hmacOffset);
    sendFn_(serialized);
}