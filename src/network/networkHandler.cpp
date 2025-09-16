#include "network/networkHandler.hpp"
#include "protocol/messages.hpp"
#include "protocol/serialize.hpp"
#include "protocol/statusCodes.hpp"

#include <arpa/inet.h>
#include <iostream>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <stdexcept>

void NetworkHandler::onMessage(int fd) {
    Session* session = sessionManager_.getSession(fd);
    if (!session) {
        return;
    }

    while (true) {
        std::optional<MessageHeader> headerOpt = peekHeader_(*session);
        if (!headerOpt) {
            return;
        }
        MessageHeader header = headerOpt.value();
        MessageType type = static_cast<MessageType>(header.messageType);

        size_t totalSize = 0;

        switch (type) {
        case MessageType::HELLO: {
            totalSize = constants::HEADER_SIZE + constants::PayloadSize::HELLO;
            if (session->recvBuffer.size() < totalSize) {
                return; // wait for more data
            }

            const uint8_t* expectedHMAC =
                session->recvBuffer.data() + constants::DataSize::HELLO;

            if (verifyHMAC_(session->hmacKey, session->recvBuffer.data(),
                            constants::DataSize::HELLO, expectedHMAC,
                            constants::HMAC_SIZE)) {
                if (auto msgOpt = deserializeMessage<HelloPayload>(session->recvBuffer)) {
                    sendFn_(*session, api_.handleHello(*session, msgOpt.value()));
                }
            }
            break;
        }
        case MessageType::LOGOUT: {
            totalSize = constants::HEADER_SIZE + constants::PayloadSize::LOGOUT;
            if (session->recvBuffer.size() < totalSize) {
                return;
            }

            const uint8_t* expectedHMAC =
                session->recvBuffer.data() + constants::DataSize::LOGOUT;

            if (verifyHMAC_(session->hmacKey, session->recvBuffer.data(),
                            constants::DataSize::LOGOUT, expectedHMAC,
                            constants::HMAC_SIZE)) {
                if (auto msgOpt =
                        deserializeMessage<LogoutPayload>(session->recvBuffer)) {
                    sendFn_(*session, api_.handleLogout(*session, msgOpt.value()));
                }
            }
            break;
        }
        case MessageType::NEW_ORDER: {
            totalSize = constants::HEADER_SIZE + constants::PayloadSize::NEW_ORDER;
            if (session->recvBuffer.size() < totalSize) {
                return;
            }

            const uint8_t* expectedHMAC =
                session->recvBuffer.data() + constants::DataSize::NEW_ORDER;

            if (verifyHMAC_(session->hmacKey, session->recvBuffer.data(),
                            constants::DataSize::NEW_ORDER, expectedHMAC,
                            constants::HMAC_SIZE)) {
                if (auto msgOpt =
                        deserializeMessage<NewOrderPayload>(session->recvBuffer)) {
                    auto responses = api_.handleNewOrder(*session, msgOpt.value());
                    for (auto& response : responses) {
                        sendFn_(*(api_.getSession(response.fd)), response.data);
                    }
                }
            }
            break;
        }

        case MessageType::CANCEL_ORDER: {
            totalSize = constants::HEADER_SIZE + constants::PayloadSize::CANCEL_ORDER;
            if (session->recvBuffer.size() < totalSize) {
                return;
            }

            const uint8_t* expectedHMAC =
                session->recvBuffer.data() + constants::DataSize::CANCEL_ORDER;

            if (verifyHMAC_(session->hmacKey, session->recvBuffer.data(),
                            constants::DataSize::CANCEL_ORDER, expectedHMAC,
                            constants::HMAC_SIZE)) {
                auto msgOpt = deserializeMessage<CancelOrderPayload>(session->recvBuffer);
                if (msgOpt) {
                    sendFn_(*session, api_.handleCancel(*session, msgOpt.value()));
                }
            }

            break;
        }

        case MessageType::MODIFY_ORDER: {
            totalSize = constants::HEADER_SIZE + constants::PayloadSize::MODIFY_ORDER;

            if (session->recvBuffer.size() < totalSize) {
                return;
            }

            const uint8_t* expectedHMAC =
                session->recvBuffer.data() + constants::DataSize::MODIFY_ORDER;

            if (verifyHMAC_(session->hmacKey, session->recvBuffer.data(),
                            constants::DataSize::MODIFY_ORDER, expectedHMAC,
                            constants::HMAC_SIZE)) {
                auto msgOpt = deserializeMessage<ModifyOrderPayload>(session->recvBuffer);
                if (!msgOpt) {
                    break;
                }

                auto responses = api_.handleModify(*session, msgOpt.value());
                for (auto& response : responses) {
                    sendFn_(*(api_.getSession(response.fd)), response.data);
                }
            }
            break;
        }
        default: {
            // unknown message type: drop connection sessionManager_.dropSession(fd);
            return;
        }
        }

        session->recvBuffer.erase(session->recvBuffer.begin(),
                                  session->recvBuffer.begin() + totalSize);
    }
}

std::optional<MessageHeader> NetworkHandler::peekHeader_(Session& session) const {
    if (session.recvBuffer.size() < sizeof(MessageHeader)) {
        return std::nullopt;
    }

    MessageHeader header{};
    std::memcpy(&header, session.recvBuffer.data(), sizeof(MessageHeader));

    header.payLoadLength = ntohs(header.payLoadLength);
    header.clientMsgSqn = ntohl(header.clientMsgSqn);
    header.serverMsgSqn = ntohl(header.serverMsgSqn);

    return header;
}

// void NetworkHandler::sendRaw_(Session& session, std::span<const std::uint8_t> buffer) {
//     session.sendBuffer.insert(std::end(session.sendBuffer), std::begin(buffer),
//                               std::end(buffer));

//     // actually push the bytes over the network
// }

bool NetworkHandler::verifyHMAC_(const std::array<uint8_t, 32>& key, const uint8_t* data,
                                 size_t dataLen, const uint8_t* expectedHMAC,
                                 size_t HMACLen) {
    auto computed = computeHMAC_(key, data, dataLen);
    if (computed.size() != HMACLen) return false;

    uint8_t diff = 0;
    for (size_t i = 0; i < HMACLen; ++i) {
        diff |= computed[i] ^ expectedHMAC[i];
    }
    return diff == 0;
}

std::vector<uint8_t> NetworkHandler::computeHMAC_(const std::array<uint8_t, 32>& key,
                                                  const uint8_t* data, size_t dataLen) {
    unsigned int len = 32;

    std::vector<uint8_t> hmac(len);
    HMAC(EVP_sha256(), key.data(), key.size(), data, dataLen, hmac.data(), &len);
    return hmac;
}
