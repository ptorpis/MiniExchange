#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "auth/session.hpp"
#include "protocol/messages.hpp"

class Client {
public:
    Client(std::array<uint8_t, constants::HMAC_SIZE> hmacKey,
           std::array<uint8_t, 16> APIKey, int fd)
        : APIKey_(APIKey), session_(fd) {
        session_.hmacKey = hmacKey;
    }

    ClientSession& getSession() { return session_; }

    template <typename Payload> void sendMessage(Message<Payload> msg);

    void processIncoming();

    std::array<uint8_t, 16> getAPIKey() { return APIKey_; }
    const std::vector<uint8_t>& readRecBuffer() const { return session_.recvBuffer; }
    const std::vector<uint8_t>& readSendBuffer() const { return session_.sendBuffer; }

    void sendHello();
    void sendLogout();
    void sendTestOrder();
    void testFill();

    void appendRecvBuffer(std::span<const uint8_t> data);

    bool getAuthStatus() { return session_.authenticated; }

    void clearSendBuffer() { session_.sendBuffer.clear(); }
    void clearRecvBuffer() { session_.recvBuffer.clear(); }

private:
    std::array<uint8_t, 16> APIKey_;

    bool verifyHMAC_(const std::array<uint8_t, 32>& key, const uint8_t* data,
                     size_t dataLen, const uint8_t* expectedHMAC, size_t HMACLen);

    std::vector<uint8_t> computeHMAC_(const std::array<uint8_t, 32>& key,
                                      const uint8_t* data, size_t dataLen);
    std::optional<MessageHeader> peekHeader_() const;
    void sendRaw_(std::span<const uint8_t> buffer);

    ClientSession session_;
};