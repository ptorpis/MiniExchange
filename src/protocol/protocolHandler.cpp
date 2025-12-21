#include "protocol/protocolHandler.hpp"
#include "protocol/clientMessages.hpp"
#include "protocol/messages.hpp"
#include "protocol/serialize.hpp"
#include "protocol/serverMessages.hpp"
#include "utils/types.hpp"
#include <algorithm>
#include <cstddef>
#include <optional>

void ProtocolHandler::onMessage(int fd) {
    Session* session = sessionManager_.getSession(fd);

    if (!session) {
        return;
    }

    processMessages_(*session);
}

void ProtocolHandler::processMessages_(Session& session) {
    std::span<const std::byte> view = session.recvBuffer;
    std::size_t totalConsumed{0};

    while (view.empty()) {
        if (view.size() < sizeof(MessageHeader)) {
            break;
        }

        auto header = peekHeader_(view);
        if (!header) {
            break;
        }

        std::size_t totalMessageSize = sizeof(MessageHeader) + header->payloadLength;

        if (view.size() < totalMessageSize) {
            break;
        }

        std::span<const std::byte> messageBytes = view.subspan(0, totalMessageSize);
        std::size_t consumed = handleMessage_(session, messageBytes);

        if (consumed == 0) {
            break;
        }
    }

    if (totalConsumed > 0) {
        auto newEnd =
            std::shift_left(session.recvBuffer.begin(), session.recvBuffer.end(),
                            static_cast<std::ptrdiff_t>(totalConsumed));
        session.recvBuffer.erase(newEnd, session.recvBuffer.end());
    }
}

std::size_t ProtocolHandler::handleMessage_(Session& session,
                                            std::span<const std::byte> messageBytes) {
    MessageType type = static_cast<MessageType>(messageBytes[0]);

    switch (type) {
    case MessageType::HELLO: {
        return handleHello_(session, messageBytes);
    }
    case MessageType::LOGOUT: {
        return handleLogout_(session, messageBytes);
    }
    case MessageType::NEW_ORDER: {
        return handleNewOrder_(session, messageBytes);
    }
    case MessageType::CANCEL_ORDER: {
        return handleCancel_(session, messageBytes);
    }
    case MessageType::MODIFY_ORDER: {
        return handleModifyOrder_(session, messageBytes);
    }

    default: {
        return 0;
    }
    }
}

std::optional<MessageHeader>
ProtocolHandler::peekHeader_(std::span<const std::byte> view) const {
    if (view.size() < sizeof(MessageHeader)) {
        return std::nullopt;
    }

    MessageHeader header;
    std::span<const std::byte> headerView = view;

    header.messageType = readUint8Advance(headerView);
    header.protocolVersionFlag = readUint8Advance(headerView);
    header.payloadLength = readUint8Advance(headerView);
    header.clientMsgSqn = readUint8Advance(headerView);
    header.serverMsgSqn = readUint8Advance(headerView);
    readBytesAdvance(headerView, reinterpret_cast<std::byte*>(header.padding),
                     sizeof(header.padding));

    return header;
}

std::size_t ProtocolHandler::handleHello_(Session& session,
                                          std::span<const std::byte> messageBytes) {
    constexpr std::size_t sizeToBeConsumed =
        sizeof(MessageHeader) + sizeof(client::HelloPayload);

    if (session.recvBuffer.size() < sizeToBeConsumed) {
        return 0;
    }

    if (session.isAuthenticated()) {
        return sizeToBeConsumed;
    }

    if (auto msgOpt = deserializeMessage<client::HelloPayload>(messageBytes)) {
        sessionManager_.authenticateClient(session.fd);
    }

    // make message and insert into buffer;

    return sizeToBeConsumed;
}

template <typename Payload> inline MessageHeader makeHeader(Session& session) {
    MessageHeader header{};
    header.messageType = +Payload::traits::type;
    header.protocolVersionFlag = +(constants::HeaderFlags::PROTOCOL_VERSION);
    header.payloadLength = Payload::traits::payloadSize;
    header.clientMsgSqn = session.getClientSqn().to_integer();
    header.serverMsgSqn = session.getNextClientSqn().to_integer();
    std::memset(header.padding, 0, sizeof(header.padding));

    return header;
}

Message<server::HelloAckPayload> makeHelloAck(Session& session) {
    Message<server::HelloAckPayload> msg;
    msg.header = makeHeader<server::HelloAckPayload>(session);
    msg.payload.serverClientID = session.getClientID().to_integer();
    msg.payload.status = 1;
    std::fill(std::begin(msg.payload.padding), std::end(msg.payload.padding), 0x00);

    return msg;
}
