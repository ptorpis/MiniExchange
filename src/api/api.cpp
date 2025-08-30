#include "api/api.hpp"
#include "protocol/serialize.hpp"
#include <openssl/evp.h>
#include <openssl/hmac.h>

Session& MiniExchangeAPI::connectClient(int fd) {
    return sessionManager_.createSession(fd);
}

void MiniExchangeAPI::disconnectClient(int fd) {
    sessionManager_.removeSession(fd);
}

Session* MiniExchangeAPI::getSession(int fd) {
    return sessionManager_.getSession(fd);
}

std::vector<uint8_t>
MiniExchangeAPI::handleHello(Session& session, std::optional<Message<HelloPayload>> msg) {

    std::vector<uint8_t> ack;
    ack.reserve(sizeof(MessageHeader) + sizeof(HelloAckPayload));

    if (!msg) {
        ack = makeHelloAck_(session, statusCodes::HelloStatus::INVALID_HMAC);
        return ack;
    }

    if (session.clientSqn >= msg.value().header.clientMsgSqn) {
        ack = makeHelloAck_(session, statusCodes::HelloStatus::OUT_OF_ORDER);
        return ack;
    }

    if (!isValidAPIKey_(session, msg.value().payload.apiKey)) {
        ack = makeHelloAck_(session, statusCodes::HelloStatus::INVALID_API_KEY);
        return ack;
    }

    session.authenticated = true;
    session.clientSqn = msg.value().header.clientMsgSqn;
    ack = makeHelloAck_(session, statusCodes::HelloStatus::ACCEPTED);
    return ack;
}

std::vector<uint8_t>
MiniExchangeAPI::handleLogout(Session& session,
                              std::optional<Message<LogoutPayload>> msg) {
    std::vector<uint8_t> ack;
    ack.reserve(sizeof(MessageHeader) + sizeof(LogoutAckPayload));

    if (!msg) {
        ack = makeLogoutAck_(session, statusCodes::LogoutStatus::INVALID_HMAC);
        return ack;
    }

    if (session.clientSqn >= msg.value().header.clientMsgSqn) {
        ack = makeLogoutAck_(session, statusCodes::LogoutStatus::OUT_OF_ORDER);
        return ack;
    }

    session.clientSqn = msg.value().header.clientMsgSqn;
    session.authenticated = false;
    ack = makeLogoutAck_(session, statusCodes::LogoutStatus::ACCEPTED);
    return ack;
}

bool MiniExchangeAPI::verifyHMAC_(const std::array<uint8_t, 32>& key, const uint8_t* data,
                                  size_t dataLen, const uint8_t* expectedHMAC,
                                  size_t HMACLen) {
    auto computed = computeHMAC_(key, data, dataLen);
    if (computed.size() != HMACLen) return false;

    volatile uint8_t diff = 0;
    for (size_t i = 0; i < HMACLen; ++i) {
        diff |= computed[i] ^ expectedHMAC[i];
    }
    return diff == 0;
}

std::vector<uint8_t> MiniExchangeAPI::computeHMAC_(const std::array<uint8_t, 32>& key,
                                                   const uint8_t* data, size_t dataLen) {
    unsigned int len = 32;

    std::vector<uint8_t> hmac(len);
    HMAC(EVP_sha256(), key.data(), key.size(), data, dataLen, hmac.data(), &len);
    return hmac;
}

std::vector<uint8_t> MiniExchangeAPI::makeHelloAck_(Session& session,
                                                    statusCodes::HelloStatus status) {
    Message<HelloAckPayload> msg = MessageFactory::makeHelloAck(session, status);
    auto serialized = serializeMessage(MessageType::HELLO_ACK, msg.payload, msg.header);
    size_t dataLen = serialized.size() - constants::HMAC_SIZE;
    auto hmac = computeHMAC_(session.hmacKey, serialized.data(), dataLen);
    std::copy(hmac.begin(), hmac.end(), serialized.data() + dataLen);
    return serialized;
}

std::vector<uint8_t> MiniExchangeAPI::makeLogoutAck_(Session& session,
                                                     statusCodes::LogoutStatus status) {
    Message<LogoutAckPayload> msg = MessageFactory::makeLoutAck(session, status);
    auto serialized = serializeMessage(MessageType::LOGOUT_ACK, msg.payload, msg.header);
    size_t dataLen = serialized.size() - constants::HMAC_SIZE;
    auto hmac = computeHMAC_(session.hmacKey, serialized.data(), dataLen);
    std::copy(hmac.begin(), hmac.end(), serialized.data() + dataLen);
    return serialized;
}

bool MiniExchangeAPI::isValidAPIKey_([[maybe_unused]] Session& session,
                                     [[maybe_unused]] const uint8_t key[16]) {
    return true;
}
