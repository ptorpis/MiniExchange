#pragma once

#include "auth/session.hpp"
#include "core/order.hpp"
#include "core/trade.hpp"
#include "protocol/statusCodes.hpp"
#include "utils/utils.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

namespace constants {
inline constexpr size_t HEADER_SIZE = 16;
inline constexpr size_t HMAC_SIZE = 32;

enum class HeaderFlags : uint8_t { PROTOCOL_VERSION = 0x01 };

} // namespace constants

namespace client {
template <typename T> struct PayloadTraits;
}

namespace server {
template <typename T> struct PayloadTraits;
}

enum class MessageType : uint8_t {
    HELLO = 0x01,
    HELLO_ACK = 0x02,
    HEARTBEAT = 0x03,
    LOGOUT = 0x04,
    LOGOUT_ACK = 0x05,
    SESSION_TIMEOUT = 0x06,
    NEW_ORDER = 0x0A,
    ORDER_ACK = 0x0B,
    CANCEL_ORDER = 0x0C,
    CANCEL_ACK = 0x0D,
    MODIFY_ORDER = 0x0E,
    MODIFY_ACK = 0x0F,
    TRADE = 0x14,
    RESEND_REQUEST = 0x28,
    RESEND_RESPONSE = 0x29,
    ERROR = 0x64
};

enum class HeaderFlags : uint8_t { PROTOCOL_VERSION = 0x01 };

#pragma pack(push, 1)
struct MessageHeader {
    uint8_t messageType;
    uint8_t protocolVersionFlag;
    uint8_t reservedFlags[2];
    uint16_t payLoadLength;
    uint32_t clientMsgSqn;
    uint32_t serverMsgSqn;
    uint8_t padding[2];

    template <typename F> void iterateElements(F&& func) {
        func(messageType);
        func(protocolVersionFlag);
        func(reservedFlags);
        func(payLoadLength);
        func(clientMsgSqn);
        func(serverMsgSqn);
        func(padding);
    }
};
#pragma pack(pop)

template <typename Payload> struct Message {
    MessageHeader header;
    Payload payload;
};

// #pragma pack(push, 1)
// struct ResendRequestPayload {
//     uint32_t startSqn;
//     uint32_t endSqn;
//     uint8_t padding[8];
//     uint8_t hmac[32];
// };
// #pragma pack(pop)

// #pragma pack(push, 1)
// struct ResendResponsePayload {
//     uint64_t serverClientID;
// add later
//     uint8_t hmac[32];
// };
// #pragma pack(pop)

#pragma pack(push, 1)
struct ErrorMessagePayload {
    uint16_t errorCode;
    uint8_t padding[14];
    uint8_t hmac[32];

    template <typename F> void iterateElements(F&& func) {
        func(errorCode);
        func(padding);
        func(hmac);
    }
};
#pragma pack(pop)

namespace constants {
namespace HeaderOffset {
constexpr size_t MESSAGE_TYPE = 0;
constexpr size_t PROTOCOL_VERSION_FLAG = 1;
constexpr size_t RESERVED_FLAGS = 2;
constexpr size_t PAYLOAD_LENGTH = 4;
constexpr size_t CLIENT_MSG_SQN = 6;
constexpr size_t SERVER_MSG_SQN = 10;
constexpr size_t PADDING = 14;
} // namespace HeaderOffset

} // namespace constants