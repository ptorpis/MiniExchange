#include "client/networkClient.hpp"
#include "protocol/clientMessages.hpp"
#include "protocol/messages.hpp"
#include "protocol/serialize.hpp"
#include "sessions/clientSession.hpp"
#include "utils/types.hpp"
#include "utils/utils.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdexcept>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

NetworkClient::NetworkClient(std::string host, std::uint16_t port)
    : NetworkClient(NetworkConfig{.tradingHost = std::move(host),
                                  .tradingPort = port,
                                  .mdConfig = MDConfig{},
                                  .enableMarketData = false}) {}

NetworkClient::NetworkClient(const NetworkConfig& config)
    : session_(config.tradingHost, config.tradingPort), mdReceiver_(nullptr) {
    if (config.enableMarketData) {
        mdReceiver_ = std::make_unique<MDReceiver>(config.mdConfig);

        try {
            mdReceiver_->initialize();
            std::cout << "Market data receiver initialized on "
                      << config.mdConfig.multicastGroup << ":" << config.mdConfig.port
                      << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to initalize market data: " << e.what() << std::endl;
            mdReceiver_.reset();
        }
    }
}

NetworkClient::~NetworkClient() {
    disconnect();
}

bool NetworkClient::connect() {
    if (session_.isConnected()) {
        return true;
    }

    session_.sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (session_.sockfd < 0) {
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(session_.port);

    if (inet_pton(AF_INET, session_.host.c_str(), &serverAddr.sin_addr) <= 0) {
        close(session_.sockfd);
        session_.sockfd = -1;
        return false;
    }

    if (::connect(session_.sockfd, reinterpret_cast<sockaddr*>(&serverAddr),
                  sizeof(serverAddr)) < 0) {
        close(session_.sockfd);
        session_.sockfd = -1;
        return false;
    }

    setNonBlocking_();
    setTCPNoDelay_();

    session_.connected.store(true, std::memory_order_release);
    startMessageLoop_();

    return true;
}

void NetworkClient::startMessageLoop_() {
    running_.store(true, std::memory_order_release);
    messageThread_ = std::thread([this]() {
        messageLoop_();
    });
}

void NetworkClient::messageLoop_() {
    while (running_.load(std::memory_order_acquire)) {
        if (!session_.isConnected()) {
            break;
        }

        if (mdReceiver_) {
            mdReceiver_->receiveOne();
        }

        fd_set readfds, writefds;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_SET(session_.sockfd, &readfds);

        bool hasDataToSend = false;
        {
            std::lock_guard<std::mutex> lock(session_.sendMutex);
            hasDataToSend = !session_.sendBuffer.empty();
        }

        if (hasDataToSend) {
            FD_SET(session_.sockfd, &writefds);
        }

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100;

        int ready = ::select(session_.sockfd + 1, &readfds, &writefds, nullptr, &timeout);

        if (ready < 0) {
            if (errno != EINTR) {
                break;
            }
            continue;
        }

        if (FD_ISSET(session_.sockfd, &readfds)) {
            char buffer[4096];
            ssize_t received = ::recv(session_.sockfd, buffer, sizeof(buffer), 0);

            if (received > 0) {
                session_.recvBuffer.insert(
                    session_.recvBuffer.end(), reinterpret_cast<std::byte*>(buffer),
                    reinterpret_cast<std::byte*>(buffer + received));
                processRecvBuffer_();
            } else if (received == 0) {
                break;
            } else {
                if (errno != EAGAIN) {
                    break;
                }
            }
        }

        if (FD_ISSET(session_.sockfd, &writefds)) {
            std::lock_guard<std::mutex> lock(session_.sendMutex);

            if (!session_.sendBuffer.empty()) {
                ssize_t sent = ::send(session_.sockfd, session_.sendBuffer.data(),
                                      session_.sendBuffer.size(), MSG_NOSIGNAL);

                if (sent > 0) {
                    session_.sendBuffer.erase(session_.sendBuffer.begin(),
                                              session_.sendBuffer.begin() + sent);
                } else if (sent < 0) {
                    if (errno != EAGAIN) {
                        break;
                    }
                }
            }
        }
    }
}

void NetworkClient::disconnect() {
    stopMessageLoop_();

    if (session_.sockfd >= 0) {
        close(session_.sockfd);
        session_.sockfd = -1;
    }

    session_.connected.store(false, std::memory_order_release);
    session_.recvBuffer.clear();
    session_.sendBuffer.clear();
    session_.clientSqn = ClientSqn32{0};
    session_.serverSqn = ServerSqn32{0};
    session_.serverClientID = ClientID{0};
}

void NetworkClient::stopMessageLoop_() {
    running_.store(false, std::memory_order_release);
    if (messageThread_.joinable()) {
        messageThread_.join();
    }
}

void NetworkClient::sendHello() {
    client::HelloPayload payload{};
    std::memset(payload.padding, 0, sizeof(payload.padding));
    sendMessage_(MessageType::HELLO, payload);
}

void NetworkClient::sendLogout() {
    client::LogoutPayload payload{};
    payload.serverClientID = session_.serverClientID.value();
    sendMessage_(MessageType::LOGOUT, payload);
}

void NetworkClient::sendNewOrder(InstrumentID instrumentID, OrderSide side,
                                 OrderType type, Qty qty, Price price,
                                 ClientOrderID clientOrderID, TimeInForce timeInForce,
                                 Timestamp goodTillDate) {
    client::NewOrderPayload payload{};
    payload.serverClientID = session_.serverClientID.value();
    payload.clientOrderID = clientOrderID.value();
    payload.instrumentID = instrumentID.value();
    payload.orderSide = +side;
    payload.orderType = +type;
    payload.timeInForce = +timeInForce;
    payload.padding = 0;
    payload.qty = qty.value();
    payload.price = price.value();
    payload.goodTillDate = goodTillDate;

    sendMessage_(MessageType::NEW_ORDER, payload);
}

void NetworkClient::sendCancel(ClientOrderID clientOrderID, OrderID serverOrderID,
                               InstrumentID instrumentID) {
    client::CancelOrderPayload payload{};
    payload.serverClientID = session_.serverClientID.value();
    payload.serverOrderID = serverOrderID.value();
    payload.clientOrderID = clientOrderID.value();
    payload.instrumentID = instrumentID.value();
    std::memset(payload.padding, 0, sizeof(payload.padding));

    sendMessage_(MessageType::CANCEL_ORDER, payload);
}

void NetworkClient::sendModify(ClientOrderID clientOrderID, OrderID serverOrderID,
                               Qty newQty, Price newPrice, InstrumentID instrumentID) {
    client::ModifyOrderPayload payload{};
    payload.serverClientID = session_.serverClientID.value();
    payload.serverOrderID = serverOrderID.value();
    payload.clientOrderID = clientOrderID.value();
    payload.newQty = newQty.value();
    payload.newPrice = newPrice.value();
    payload.instrumentID = instrumentID.value();
    std::memset(payload.padding, 0, sizeof(payload.padding));

    sendMessage_(MessageType::MODIFY_ORDER, payload);
}

template <typename Payload>
void NetworkClient::sendMessage_(MessageType type, const Payload& payload) {
    MessageHeader header;
    header.messageType = +type;
    header.protocolVersionFlag = MessageHeader::traits::PROTOCOL_VERSION;
    header.payloadLength = Payload::traits::payloadSize;
    header.clientMsgSqn = (++session_.clientSqn).value();
    header.serverMsgSqn = session_.serverSqn.value();
    std::memset(header.padding, 0, sizeof(header.padding));

    std::lock_guard<std::mutex> lock(session_.sendMutex);
    serializeMessageInto(session_.sendBuffer, type, header, payload);
}

void NetworkClient::processRecvBuffer_() {
    std::span<const std::byte> view = session_.recvBuffer;
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
        auto newEnd =
            std::shift_left(session_.recvBuffer.begin(), session_.recvBuffer.end(),
                            static_cast<std::ptrdiff_t>(totalConsumed));

        session_.recvBuffer.erase(newEnd, session_.recvBuffer.end());
    }
}

void NetworkClient::handleMessage_(std::span<const std::byte> messageBytes) {
    MessageType type =
        static_cast<MessageType>(std::to_integer<std::uint8_t>(messageBytes[0]));

    switch (type) {
    case MessageType::HELLO_ACK:
        if (auto msg = deserializeMessage<server::HelloAckPayload>(messageBytes)) {
            session_.serverSqn = ServerSqn32{msg->header.serverMsgSqn};
            session_.serverClientID = ClientID{msg->payload.serverClientID};

            if (helloAckCallback_) {
                helloAckCallback_(*msg);
            }
        }
        break;

    case MessageType::LOGOUT_ACK:
        if (auto msg = deserializeMessage<server::LogoutAckPayload>(messageBytes)) {
            session_.serverSqn = ServerSqn32{msg->header.serverMsgSqn};

            if (logoutAckCallback_) {
                logoutAckCallback_(*msg);
            }
        }
        break;

    case MessageType::ORDER_ACK:
        if (auto msg = deserializeMessage<server::OrderAckPayload>(messageBytes)) {
            session_.serverSqn = ServerSqn32{msg->header.serverMsgSqn};

            if (orderAckCallback_) {
                orderAckCallback_(*msg);
            }
        }
        break;

    case MessageType::CANCEL_ACK:
        if (auto msg = deserializeMessage<server::CancelAckPayload>(messageBytes)) {
            session_.serverSqn = ServerSqn32{msg->header.serverMsgSqn};

            if (cancelAckCallback_) {
                cancelAckCallback_(*msg);
            }
        }
        break;

    case MessageType::MODIFY_ACK:
        if (auto msg = deserializeMessage<server::ModifyAckPayload>(messageBytes)) {
            session_.serverSqn = ServerSqn32{msg->header.serverMsgSqn};

            if (modifyAckCallback_) {
                modifyAckCallback_(*msg);
            }
        }
        break;

    case MessageType::TRADE:
        if (auto msg = deserializeMessage<server::TradePayload>(messageBytes)) {
            session_.serverSqn = ServerSqn32{msg->header.serverMsgSqn};

            if (tradeCallback_) {
                tradeCallback_(*msg);
            }
        }
        break;

    default:
        break;
    }
}

void NetworkClient::setNonBlocking_() {
    int flags = fcntl(session_.sockfd, F_GETFL, 0);
    if (flags < 0) {
        throw std::runtime_error("fcntl F_GETFL failed");
    }

    if (fcntl(session_.sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        throw std::runtime_error("fcntl F_SETFL failed");
    }
}

void NetworkClient::setTCPNoDelay_() {
    int flag = 1;
    setsockopt(session_.sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
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
