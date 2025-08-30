#include "client/client.hpp"
#include "protocol/serialize.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <openssl/evp.h>
#include <openssl/hmac.h>

void Client::sendHello() {
    Message<HelloPayload> msg;
    msg.header = makeClientHeader<HelloPayload>(session_);
    std::memcpy(msg.payload.apiKey, getAPIKey().data(), getAPIKey().size());
    std::fill(std::begin(msg.payload.hmac), std::end(msg.payload.hmac), 0x00);
    auto serialized = serializeMessage(MessageType::HELLO, msg.payload, msg.header);

    size_t HMACInputSize =
        constants::HEADER_SIZE + constants::PayloadSize::HELLO - constants::HMAC_SIZE;
    auto hmac = computeHMAC_(session_.hmacKey, serialized.data(), HMACInputSize);

    std::copy(hmac.begin(), hmac.end(),
              serialized.data() + constants::HMACOffset::HELLO_OFFSET);

    session_.sendBuffer.insert(session_.sendBuffer.end(), serialized.begin(),
                               serialized.end());
}

void Client::sendLogout() {}

void Client::processIncoming() {
    if (session_.recvBuffer.empty()) return;
    std::optional<MessageHeader> headerOpt = peekHeader_();

    if (!headerOpt) {
        return;
    }

    const MessageHeader& parsedHeader = headerOpt.value();

    if (session_.recvBuffer.size() <
        parsedHeader.payLoadLength + constants::HEADER_SIZE) {
        return;
    }

    session_.serverSqn++;

    switch (static_cast<MessageType>(parsedHeader.messageType)) {
    case MessageType::HELLO_ACK: {
        size_t msgSize =
            constants::HEADER_SIZE + parsedHeader.payLoadLength - constants::HMAC_SIZE;
        const uint8_t* expectedHMAC = session_.recvBuffer.data() + msgSize;

        bool ok = verifyHMAC_(session_.hmacKey, session_.recvBuffer.data(), msgSize,
                              expectedHMAC, constants::HMAC_SIZE);
        if (!ok) {
            return;
        } else {
            std::optional<Message<HelloAckPayload>> helloAck =
                deserializeMessage<HelloAckPayload>(session_.recvBuffer);
            if (!helloAck) {
                return;
            }
            if (static_cast<statusCodes::HelloStatus>(helloAck.value().payload.status) ==
                statusCodes::HelloStatus::ACCEPTED) {
                session_.authenticated = true;
                session_.serverClientID = helloAck.value().payload.serverClientID;
            } else {
                return;
            }
        }

        break;
    }

    default: {
        break;
    }
    }
}

std::optional<MessageHeader> Client::peekHeader_() const {
    if (session_.recvBuffer.size() < sizeof(MessageHeader)) {
        return std::nullopt;
    }

    MessageHeader header{};
    std::memcpy(&header, session_.recvBuffer.data(), sizeof(MessageHeader));

    header.payLoadLength = ntohs(header.payLoadLength);
    header.clientMsgSqn = ntohl(header.clientMsgSqn);
    header.serverMsgSqn = ntohl(header.serverMsgSqn);

    return header;
}

bool Client::verifyHMAC_(const std::array<uint8_t, 32>& key, const uint8_t* data,
                         size_t dataLen, const uint8_t* expectedHMAC, size_t HMACLen) {
    auto computed = computeHMAC_(key, data, dataLen);
    if (computed.size() != HMACLen) return false;

    volatile uint8_t diff = 0;
    for (size_t i = 0; i < HMACLen; ++i) {
        diff |= computed[i] ^ expectedHMAC[i];
    }
    return diff == 0;
}

std::vector<uint8_t> Client::computeHMAC_(const std::array<uint8_t, 32>& key,
                                          const uint8_t* data, size_t dataLen) {
    unsigned int len = 32;

    std::vector<uint8_t> hmac(len);
    HMAC(EVP_sha256(), key.data(), key.size(), data, dataLen, hmac.data(), &len);
    return hmac;
}

void Client::appendRecvBuffer(std::span<const uint8_t> data) {
    session_.recvBuffer.insert(std::end(session_.recvBuffer), std::begin(data),
                               std::end(data));
}