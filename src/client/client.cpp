#include "client/client.hpp"
#include <arpa/inet.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <utility>

void Client::sendHello() {
    Message<HelloPayload> msg;
    msg.header = makeClientHeader<HelloPayload>(session_);
    std::memcpy(msg.payload.apiKey, getAPIKey().data(), getAPIKey().size());
    std::fill(std::begin(msg.payload.hmac), std::end(msg.payload.hmac), 0x00);
    sendMessage(msg);
}

void Client::sendLogout() {
    Message<LogoutPayload> msg;
    msg.header = makeClientHeader<LogoutPayload>(session_);
    msg.payload.serverClientID = session_.serverClientID;
    std::fill(std::begin(msg.payload.padding), std::end(msg.payload.padding), 0x00);
    sendMessage(msg);
}

void Client::sendCancel(OrderID orderID) {
    Message<CancelOrderPayload> msg;
    msg.header = makeClientHeader<CancelOrderPayload>(session_);
    msg.payload.serverClientID = session_.serverClientID;
    msg.payload.serverOrderID = orderID;

    std::fill(std::begin(msg.payload.padding), std::end(msg.payload.padding), 0x00);
    sendMessage(msg);
}

void Client::sendModify(OrderID orderID, Qty newQty, Price newPrice) {
    Message<ModifyOrderPayload> msg;
    msg.header = makeClientHeader<ModifyOrderPayload>(session_);

    msg.payload.serverClientID = session_.serverClientID;
    msg.payload.serverOrderID = orderID;

    msg.payload.newQuantity = newQty;
    msg.payload.newPrice = newPrice;
    sendMessage(msg);
}

void Client::processIncoming() {
    size_t totalSize = 0;
    while (true) {
        std::optional<MessageHeader> headerOpt = peekHeader_();
        if (!headerOpt) {
            return;
        }

        MessageHeader header = headerOpt.value();
        MessageType type = static_cast<MessageType>(header.messageType);
        switch (type) {
        case MessageType::HELLO_ACK: {
            totalSize = constants::HEADER_SIZE + constants::PayloadSize::HELLO;
            if (session_.recvBuffer.size() < totalSize) {
                return;
            }

            const uint8_t* expectedHMAC =
                session_.recvBuffer.data() + constants::DataSize::HELLO_ACK;

            if (verifyHMAC_(session_.hmacKey, session_.recvBuffer.data(),
                            constants::DataSize::HELLO_ACK, expectedHMAC,
                            constants::HMAC_SIZE)) {
                auto msgOpt = deserializeMessage<HelloAckPayload>(session_.recvBuffer);
                if (!msgOpt) {
                    break;
                }

                if (statusCodes::HelloStatus::ACCEPTED ==
                    static_cast<statusCodes::HelloStatus>(
                        msgOpt.value().payload.status)) {
                    session_.authenticated = true;
                    session_.clientSqn = msgOpt.value().header.serverMsgSqn;
                    session_.serverClientID = msgOpt.value().payload.serverClientID;
                }
            }
            break;
        }
        case MessageType::LOGOUT_ACK: {
            totalSize = constants::HEADER_SIZE + constants::PayloadSize::LOGOUT_ACK;
            if (session_.recvBuffer.size() < totalSize) {
                return;
            }
            const uint8_t* expectedHMAC =
                session_.recvBuffer.data() + constants::DataSize::LOGOUT_ACK;

            if (verifyHMAC_(session_.hmacKey, session_.recvBuffer.data(),
                            constants::DataSize::LOGOUT_ACK, expectedHMAC,
                            constants::HMAC_SIZE)) {
                auto msgOpt = deserializeMessage<LogoutAckPayload>(session_.recvBuffer);

                if (!msgOpt) {
                    break;
                }

                if (statusCodes::LogoutStatus::ACCEPTED ==
                    static_cast<statusCodes::LogoutStatus>(
                        msgOpt.value().payload.status)) {
                    session_.authenticated = false;
                    session_.clientSqn = msgOpt.value().header.clientMsgSqn;
                }
            }

            break;
        }

        case MessageType::ORDER_ACK: {
            totalSize = constants::HEADER_SIZE + constants::PayloadSize::ORDER_ACK;
            if (session_.recvBuffer.size() < totalSize) {
                return;
            }
            const uint8_t* expectedHMAC =
                session_.recvBuffer.data() + constants::DataSize::ORDER_ACK;

            if (verifyHMAC_(session_.hmacKey, session_.recvBuffer.data(),
                            constants::DataSize::ORDER_ACK, expectedHMAC,
                            constants::HMAC_SIZE)) {
                auto msgOpt = deserializeMessage<LogoutAckPayload>(session_.recvBuffer);

                if (!msgOpt) {
                    break;
                }
                session_.serverSqn = msgOpt.value().header.serverMsgSqn;
                std::cout << "Order Accepted" << std::endl;
                break;
            }
            break;
        }

        case MessageType::TRADE: {
            totalSize = constants::HEADER_SIZE + constants::PayloadSize::TRADE;
            if (session_.recvBuffer.size() < totalSize) {
                return;
            }

            std::array<uint8_t, constants::HMAC_SIZE> expectedHMAC{0};
            std::memcpy(expectedHMAC.data(),
                        session_.recvBuffer.data() + constants::DataSize::TRADE,
                        constants::HMAC_SIZE);

            if (verifyHMAC_(session_.hmacKey, session_.recvBuffer.data(),
                            constants::DataSize::TRADE, expectedHMAC.data(),
                            constants::HMAC_SIZE)) {
                auto msgOpt = deserializeMessage<TradePayload>(session_.recvBuffer);

                if (!msgOpt) {
                    break;
                }

                session_.serverSqn = msgOpt.value().header.serverMsgSqn;
                std::cout << "Trade Accepted" << std::endl;
                session_.exeID++;
                break;
            }
            break;
        }

        case MessageType::CANCEL_ACK: {
            totalSize = constants::HEADER_SIZE + constants::PayloadSize::CANCEL_ACK;
            if (session_.recvBuffer.size() < totalSize) {
                return;
            }

            std::array<uint8_t, constants::HMAC_SIZE> expectedHMAC{0};
            std::memcpy(expectedHMAC.data(),
                        session_.recvBuffer.data() + constants::DataSize::CANCEL_ACK,
                        constants::HMAC_SIZE);

            if (verifyHMAC_(session_.hmacKey, session_.recvBuffer.data(),
                            constants::DataSize::CANCEL_ACK, expectedHMAC.data(),
                            constants::HMAC_SIZE)) {
                auto msgOpt = deserializeMessage<CancelAckPayload>(session_.recvBuffer);

                if (!msgOpt) {
                    break;
                }

                session_.serverSqn = msgOpt.value().header.serverMsgSqn;
                if (static_cast<statusCodes::CancelAckStatus>(
                        msgOpt.value().payload.status) ==
                    statusCodes::CancelAckStatus::ACCEPTED) {
                    std::cout << "Order cancellation accepted" << std::endl;
                } else {
                    std::cout << "Order cancellation rejected" << std::endl;
                }

                break;
            }
            break;
        }

        case MessageType::MODIFY_ACK: {
            totalSize = constants::HEADER_SIZE + constants::PayloadSize::MODIFY_ACK;
            if (session_.recvBuffer.size() < totalSize) {
                return;
            }

            std::array<uint8_t, constants::HMAC_SIZE> expectedHMAC{0};
            std::memcpy(expectedHMAC.data(),
                        session_.recvBuffer.data() + constants::DataSize::CANCEL_ACK,
                        constants::HMAC_SIZE);

            if (verifyHMAC_(session_.hmacKey, session_.recvBuffer.data(),
                            constants::DataSize::CANCEL_ACK, expectedHMAC.data(),
                            constants::HMAC_SIZE)) {
                auto msgOpt = deserializeMessage<ModifyAckPayload>(session_.recvBuffer);

                if (!msgOpt) {
                    break;
                }

                session_.serverSqn = msgOpt.value().header.serverMsgSqn;
                if (static_cast<statusCodes::ModifyStatus>(
                        msgOpt.value().payload.status) ==
                    statusCodes::ModifyStatus::ACCEPTED) {
                    std::cout << "Modify Successful" << std::endl;
                } else {
                    std::cout << "Modify Unsuccessful" << std::endl;
                }
            }

            break;
        }

        default: {
            break;
        }
        }

        session_.recvBuffer.erase(session_.recvBuffer.begin(),
                                  session_.recvBuffer.begin() + totalSize);

        if (!session_.recvBuffer.size()) {
            return;
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

void Client::sendTestOrder() {
    Message<NewOrderPayload> msg;
    msg.header = makeClientHeader<NewOrderPayload>(session_);
    msg.payload.serverClientID = session_.serverClientID;
    msg.payload.instrumentID = 1;
    msg.payload.orderSide = std::to_underlying(OrderSide::BUY);
    msg.payload.orderType = std::to_underlying(OrderType::LIMIT);
    msg.payload.quantity = 100;
    msg.payload.price = 200;
    msg.payload.timeInForce = std::to_underlying(TimeInForce::GTC);
    msg.payload.goodTillDate = std::numeric_limits<Timestamp>::max();
    std::fill(std::begin(msg.payload.hmac), std::end(msg.payload.hmac), 0x00);
    std::fill(std::begin(msg.payload.padding), std::end(msg.payload.padding), 0x00);

    sendMessage(msg);
}

void Client::testFill() {
    Message<NewOrderPayload> msg;
    msg.header = makeClientHeader<NewOrderPayload>(session_);
    msg.payload.serverClientID = session_.serverClientID;
    msg.payload.instrumentID = 1;
    msg.payload.orderSide = std::to_underlying(OrderSide::SELL);
    msg.payload.orderType = std::to_underlying(OrderType::LIMIT);
    msg.payload.quantity = 100;
    msg.payload.price = 200;
    msg.payload.timeInForce = std::to_underlying(TimeInForce::GTC);
    msg.payload.goodTillDate = std::numeric_limits<Timestamp>::max();
    std::fill(std::begin(msg.payload.hmac), std::end(msg.payload.hmac), 0x00);
    std::fill(std::begin(msg.payload.padding), std::end(msg.payload.padding), 0x00);

    sendMessage(msg);
}