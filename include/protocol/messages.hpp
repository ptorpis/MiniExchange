#pragma once

#include "utils/utils.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string_view>

namespace constants {
inline constexpr size_t HEADER_SIZE = 16;

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

inline std::ostream& operator<<(std::ostream& os, MessageType type) {
    std::string_view name;
    switch (type) {
    case MessageType::HELLO:
        name = "HELLO";
        break;
    case MessageType::HELLO_ACK:
        name = "HELLO_ACK";
        break;
    case MessageType::HEARTBEAT:
        name = "HEARTBEAT";
        break;
    case MessageType::LOGOUT:
        name = "LOGOUT";
        break;
    case MessageType::LOGOUT_ACK:
        name = "LOGOUT_ACK";
        break;
    case MessageType::SESSION_TIMEOUT:
        name = "SESSION_TIMEOUT";
        break;
    case MessageType::NEW_ORDER:
        name = "NEW_ORDER";
        break;
    case MessageType::ORDER_ACK:
        name = "ORDER_ACK";
        break;
    case MessageType::CANCEL_ORDER:
        name = "CANCEL_ORDER";
        break;
    case MessageType::CANCEL_ACK:
        name = "CANCEL_ACK";
        break;
    case MessageType::MODIFY_ORDER:
        name = "MODIFY_ORDER";
        break;
    case MessageType::MODIFY_ACK:
        name = "MODIFY_ACK";
        break;
    case MessageType::TRADE:
        name = "TRADE";
        break;
    case MessageType::RESEND_REQUEST:
        name = "RESEND_REQUEST";
        break;
    case MessageType::RESEND_RESPONSE:
        name = "RESEND_RESPONSE";
        break;
    case MessageType::ERROR:
        name = "ERROR";
        break;
    default:
        name = "UNKNOWN";
        break;
    }

    os << name << " (0x" << std::hex << std::uppercase << static_cast<int>(+type)
       << std::dec << ")";
    return os;
}

enum class HeaderFlags : uint8_t { PROTOCOL_VERSION = 0x01 };

#pragma pack(push, 1)
struct MessageHeader {
    uint8_t messageType;
    uint8_t protocolVersionFlag;
    uint16_t payLoadLength;
    uint32_t clientMsgSqn;
    uint32_t serverMsgSqn;
    uint8_t padding[4];

    template <typename F> void iterateElements(F&& func) {
        func(messageType);
        func(protocolVersionFlag);
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

#pragma pack(push, 1)
struct ErrorMessagePayload {
    uint16_t errorCode;
    uint8_t padding[14];

    template <typename F> void iterateElements(F&& func) {
        func(errorCode);
        func(padding);
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
