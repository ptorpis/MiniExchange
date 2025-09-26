#pragma once

#include "protocol/messages.hpp"
#include "utils/utils.hpp"

namespace client {
template <typename Payload>
inline MessageHeader makeClientHeader(ClientSession& session) {
    MessageHeader header{};
    header.messageType = +PayloadTraits<Payload>::type;
    header.protocolVersionFlag = +constants::HeaderFlags::PROTOCOL_VERSION;
    header.payLoadLength = static_cast<uint16_t>(PayloadTraits<Payload>::size);
    header.clientMsgSqn = ++session.clientSqn;
    header.serverMsgSqn = session.serverSqn;
    std::memset(header.reservedFlags, 0, sizeof(header.reservedFlags));
    std::memset(header.padding, 0, sizeof(header.padding));

    return header;
}
} // namespace client