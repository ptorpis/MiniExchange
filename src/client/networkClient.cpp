#include "client/networkClient.hpp"
#include "protocol/clientMessages.hpp"
#include "protocol/messages.hpp"
#include "protocol/serialize.hpp"
#include "protocol/serverMessages.hpp"
#include "utils/types.hpp"
#include "utils/utils.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

NetworkClient::NetworkClient(std::string host, std::uint16_t port)
    : sockfd_(-1), host_(std::move(host)), port_(port) {
    recvBuffer_.reserve(4 * 1024);
    sendBuffer_.reserve(4 * 1024);
}

NetworkClient::~NetworkClient() {
    disconnect();
}

bool NetworkClient::connect() {
    if (isConnected()) {
        return true;
    }

    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_ < 0) {
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port_);

    if (inet_pton(AF_INET, host_.c_str(), &serverAddr.sin_addr) <= 0) {
        close(sockfd_);
        sockfd_ = -1;
        return false;
    }

    if (::connect(sockfd_, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) <
        0) {
        close(sockfd_);
        sockfd_ = -1;
        return false;
    }

    setNonBlocking_();
    setTCPNoDelay_();

    return true;
}

void NetworkClient::disconnect() {
    if (sockfd_ >= 0) {
        close(sockfd_);
        sockfd_ = -1;
    }

    recvBuffer_.clear();
    sendBuffer_.clear();

    clientSqn = ClientSqn32{0};
    serverSqn = ServerSqn32{0};
}

void NetworkClient::sendHello() {
    client::HelloPayload payload{};
    std::memset(payload.padding, 0, sizeof(payload.padding));
    sendMessage_(MessageType::HELLO, payload);
}

void NetworkClient::sendLogout() {
    client::LogoutPayload payload{};
    payload.serverClientID = serverClientID.value();
    sendMessage_(MessageType::LOGOUT, payload);
}

void NetworkClient::sendNewOrder(const client::NewOrderPayload& payload) {
    sendMessage_(MessageType::NEW_ORDER, payload);
}

void NetworkClient::sendCancel(const client::CancelOrderPayload& payload) {
    sendMessage_(MessageType::CANCEL_ORDER, payload);
}

void NetworkClient::sendModify(const client::ModifyOrderPayload& payload) {
    sendMessage_(MessageType::MODIFY_ORDER, payload);
}

template <typename Payload>
void NetworkClient::sendMessage_(MessageType type, const Payload& payload) {
    MessageHeader header;
    header.messageType = +type;
    header.protocolVersionFlag = MessageHeader::traits::PROTOCOL_VERSION;
    header.payloadLength = Payload::traits::payloadSize;
    header.clientMsgSqn = (++clientSqn).value();
    header.serverMsgSqn = serverSqn.value();

    std::memset(header.padding, 0, sizeof(header.padding));
    Message<Payload> message = {header, payload};

    utils::printMessage(std::cout, message);

    serializeMessageInto(sendBuffer_, type, header, payload);

    while (!sendBuffer_.empty()) {
        ssize_t sent =
            ::send(sockfd_, sendBuffer_.data(), sendBuffer_.size(), MSG_NOSIGNAL);

        if (sent > 0) {
            sendBuffer_.erase(sendBuffer_.begin(), sendBuffer_.begin() + sent);
        } else if (sent < 0) {
            if (errno == EAGAIN) {
                break;
            } else if (errno != EINTR) {
                throw std::runtime_error("Send failed: " + std::string(strerror(errno)));
            }
        }
    }
}

void NetworkClient::processMessages() {
    if (!isConnected()) {
        return;
    }

    while (!sendBuffer_.empty()) {
        ssize_t sent =
            send(sockfd_, sendBuffer_.data(), sendBuffer_.size(), MSG_NOSIGNAL);

        if (sent > 0) {
            sendBuffer_.erase(sendBuffer_.begin(), sendBuffer_.begin() + sent);
        } else if (sent < 0) {
            if (errno == EAGAIN) {
                break;
            } else if (errno != EINTR) {
                disconnect();
                return;
            }
        }
    }

    char buffer[4096];
    ssize_t received = recv(sockfd_, buffer, sizeof(buffer), 0);

    if (received > 0) {
        recvBuffer_.insert(recvBuffer_.end(), reinterpret_cast<std::byte*>(buffer),
                           reinterpret_cast<std::byte*>(buffer + received));

        processRecvBuffer_();

    } else if (received == 0) {
        disconnect();

    } else {
        if (errno != EAGAIN && errno != EINTR) {
            disconnect();
        }
    }
}

void NetworkClient::processRecvBuffer_() {
    std::span<const std::byte> view = recvBuffer_;
    std::size_t totalConsumed = 0;

    while (!view.empty()) {
        if (view.size() < sizeof(MessageHeader)) {
            break;
        }

        std::uint16_t payloadLength;
        std::memcpy(&payloadLength, view.data() + 2, sizeof(std::uint16_t));
        payloadLength = swapEndian(payloadLength);

        std::size_t totalSize = sizeof(MessageHeader) + payloadLength;

        if (view.size() < totalSize) {
            break;
        }

        std::span<const std::byte> messageBytes = view.subspan(0, totalSize);

        handleMessage_(messageBytes);

        view = view.subspan(totalSize);
        totalConsumed += totalSize;
    }

    if (totalConsumed > 0) {
        auto newEnd = std::shift_left(recvBuffer_.begin(), recvBuffer_.end(),
                                      static_cast<std::ptrdiff_t>(totalConsumed));

        recvBuffer_.erase(newEnd, recvBuffer_.end());
    }
}

void NetworkClient::handleMessage_(std::span<const std::byte> messageBytes) {
    MessageType type =
        static_cast<MessageType>(std::to_integer<std::uint8_t>(messageBytes[0]));
    switch (type) {
    case MessageType::HELLO_ACK:
        if (auto msg = deserializeMessage<server::HelloAckPayload>(messageBytes)) {
            serverSqn = ServerSqn32{msg->header.serverMsgSqn};
            onHelloAck(*msg);
        }
        break;

    case MessageType::LOGOUT_ACK:
        if (auto msg = deserializeMessage<server::LogoutAckPayload>(messageBytes)) {
            serverSqn = ServerSqn32{msg->header.serverMsgSqn};
            onLogoutAck(*msg);
        }
        break;

    case MessageType::ORDER_ACK:
        if (auto msg = deserializeMessage<server::OrderAckPayload>(messageBytes)) {
            serverSqn = ServerSqn32{msg->header.serverMsgSqn};
            onOrderAck(*msg);
        }
        break;

    case MessageType::CANCEL_ACK:
        if (auto msg = deserializeMessage<server::CancelAckPayload>(messageBytes)) {
            serverSqn = ServerSqn32{msg->header.serverMsgSqn};
            onCancelAck(*msg);
        }
        break;

    case MessageType::MODIFY_ACK:
        if (auto msg = deserializeMessage<server::ModifyAckPayload>(messageBytes)) {
            serverSqn = ServerSqn32{msg->header.serverMsgSqn};
            onModifyAck(*msg);
        }
        break;

    case MessageType::TRADE:
        if (auto msg = deserializeMessage<server::TradePayload>(messageBytes)) {
            serverSqn = ServerSqn32{msg->header.serverMsgSqn};
            onTrade(*msg);
        }
        break;

    default:
        break;
    }
}

void NetworkClient::setNonBlocking_() {
    int flags = fcntl(sockfd_, F_GETFL, 0);
    if (flags < 0) {
        throw std::runtime_error("fcntl F_GETFL failed");
    }

    if (fcntl(sockfd_, F_SETFL, flags | O_NONBLOCK) < 0) {
        throw std::runtime_error("fcntl F_SETFL failed");
    }
}

void NetworkClient::setTCPNoDelay_() {
    int flag = 1;
    setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
}

template void
NetworkClient::sendMessage_<client::HelloPayload>(MessageType,
                                                  const client::HelloPayload&);
template void
NetworkClient::sendMessage_<client::LogoutPayload>(MessageType,
                                                   const client::LogoutPayload&);
template void
NetworkClient::sendMessage_<client::NewOrderPayload>(MessageType,
                                                     const client::NewOrderPayload&);
template void NetworkClient::sendMessage_<client::CancelOrderPayload>(
    MessageType, const client::CancelOrderPayload&);
template void NetworkClient::sendMessage_<client::ModifyOrderPayload>(
    MessageType, const client::ModifyOrderPayload&);
