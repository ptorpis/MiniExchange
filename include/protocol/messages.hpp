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

// namespace PayloadSize {
// inline constexpr size_t HELLO = sizeof(HelloPayload);
// inline constexpr size_t HELLO_ACK = sizeof(HelloAckPayload);
// inline constexpr size_t HEARTBEAT = sizeof(HeartBeatPayload);
// inline constexpr size_t LOGOUT = sizeof(LogoutPayload);
// inline constexpr size_t LOGOUT_ACK = sizeof(LogoutAckPayload);
// inline constexpr size_t SESSION_TIMEOUT = sizeof(SessionTimeoutPayload);
// inline constexpr size_t NEW_ORDER = sizeof(NewOrderPayload);
// inline constexpr size_t ORDER_ACK = sizeof(OrderAckPayload);
// inline constexpr size_t CANCEL_ORDER = sizeof(CancelOrderPayload);
// inline constexpr size_t CANCEL_ACK = sizeof(CancelAckPayload);
// inline constexpr size_t MODIFY_ORDER = sizeof(ModifyOrderPayload);
// inline constexpr size_t MODIFY_ACK = sizeof(ModifyAckPayload);
// inline constexpr size_t TRADE = sizeof(TradePayload);
// } // namespace PayloadSize

// // header + payload - HMAC
// namespace DataSize {
// inline constexpr size_t HELLO = HEADER_SIZE + PayloadSize::HELLO - HMAC_SIZE;
// inline constexpr size_t HELLO_ACK = HEADER_SIZE + PayloadSize::HELLO_ACK - HMAC_SIZE;
// inline constexpr size_t HEARTBEAT = HEADER_SIZE + PayloadSize::HEARTBEAT - HMAC_SIZE;
// inline constexpr size_t LOGOUT = HEADER_SIZE + PayloadSize::LOGOUT - HMAC_SIZE;
// inline constexpr size_t LOGOUT_ACK = HEADER_SIZE + PayloadSize::LOGOUT_ACK - HMAC_SIZE;
// inline constexpr size_t SESSION_TIMEOUT =
//     HEADER_SIZE + PayloadSize::SESSION_TIMEOUT - HMAC_SIZE;
// inline constexpr size_t NEW_ORDER = HEADER_SIZE + PayloadSize::NEW_ORDER - HMAC_SIZE;
// inline constexpr size_t ORDER_ACK = HEADER_SIZE + PayloadSize::ORDER_ACK - HMAC_SIZE;
// inline constexpr size_t CANCEL_ORDER =
//     HEADER_SIZE + PayloadSize::CANCEL_ORDER - HMAC_SIZE;
// inline constexpr size_t CANCEL_ACK = HEADER_SIZE + PayloadSize::CANCEL_ACK - HMAC_SIZE;
// inline constexpr size_t MODIFY_ORDER =
//     HEADER_SIZE + PayloadSize::MODIFY_ORDER - HMAC_SIZE;
// inline constexpr size_t MODIFY_ACK = HEADER_SIZE + PayloadSize::MODIFY_ACK - HMAC_SIZE;
// inline constexpr size_t TRADE = HEADER_SIZE + PayloadSize::TRADE - HMAC_SIZE;
// } // namespace DataSize

// namespace HMACOffset {
// constexpr size_t HELLO_OFFSET = HEADER_SIZE + offsetof(HelloPayload, hmac);
// constexpr size_t LOGOUT_OFFSET = HEADER_SIZE + offsetof(LogoutPayload, hmac);
// constexpr size_t CANCEL_OFFSET = HEADER_SIZE + offsetof(CancelOrderPayload, hmac);
// constexpr size_t MODIFY_OFFSET = HEADER_SIZE + offsetof(ModifyOrderPayload, hmac);
// } // namespace HMACOffset

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
// static_assert(sizeof(MessageHeader) == 16, "MessageHeader size incorrect");
// static_assert(sizeof(HelloPayload) == 48, "HelloPayload size incorrect");
// static_assert(sizeof(HelloAckPayload) == 48, "HelloAckPayload size incorrect");
// static_assert(sizeof(HeartBeatPayload) == 48, "HeartBeatPayload size incorrect");
// static_assert(sizeof(LogoutPayload) == 48, "LogoutPayload size incorrect");
// static_assert(sizeof(LogoutAckPayload) == 48, "LogoutAckPayload size incorrect");
// static_assert(sizeof(SessionTimeoutPayload) == 48,
//               "SessionTimeoutPayload size incorrect");
// static_assert(sizeof(NewOrderPayload) == 80, "NewOrderPayload size incorrect");
// static_assert(sizeof(OrderAckPayload) == 80, "OrderAckPayload size incorrect");
// static_assert(sizeof(CancelOrderPayload) == 64, "CancelOrderPayload size incorrect");
// static_assert(sizeof(CancelAckPayload) == 64, "CancelAckPayload size incorrect");
// static_assert(sizeof(ModifyOrderPayload) == 64, "ModifyOrderPayload size incorrect");
// static_assert(sizeof(ModifyAckPayload) == 64, "ModifyAckPayload size incorrect");
// static_assert(sizeof(TradePayload) == 80, "TradePayload size incorrect");
// static_assert(sizeof(ErrorMessagePayload) == 48, "ErrorMessagePayload size incorrect");