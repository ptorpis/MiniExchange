#include "network/networkHandler.hpp"
#include "protocol/messages.hpp"
#include "protocol/serialize.hpp"
#include "protocol/statusCodes.hpp"

#include <arpa/inet.h>
#include <stdexcept>

void NetworkHandler::onMessage(int fd) {
    Session* session = sessionManager_.getSession(fd);
    if (!session) {
        return;
    }

    std::optional<MessageHeader> headerOpt = peekHeader_(*session);
    if (!headerOpt) {
        return;
    }
    MessageHeader header = headerOpt.value();

    if (!isMessageComplete_(*session, header.payLoadLength)) {
        return;
    }

    MessageType type = static_cast<MessageType>(header.messageType);

    switch (type) {
    case MessageType::HELLO: {
        auto msg = deserializeMessage<HelloPayload>(session->recvBuffer);
        sendRaw_(*session, api_.handleHello(*session, msg));
        break;
    }
    case MessageType::LOGOUT: {
        auto msg = deserializeMessage<LogoutPayload>(session->recvBuffer);
        sendRaw_(*session, api_.handleLogout(*session, msg));
    }
    default: {
        break;
    }
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

bool NetworkHandler::isMessageComplete_(Session& session, const uint16_t payloadLength) {
    return session.recvBuffer.size() >= sizeof(MessageHeader) + payloadLength;
}

void NetworkHandler::sendRaw_(Session& session, std::span<const std::uint8_t> buffer) {
    session.sendBuffer.insert(std::end(session.sendBuffer), std::begin(buffer),
                              std::end(buffer));

    // actually push the bytes over the network
}
