#include "client/client.hpp"
#include "protocol/serialize.hpp"
#include <arpa/inet.h>
#include <iostream>

void Client::sendHello() {}

void Client::sendLogout() {}

void Client::processIncoming() {}

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
