#pragma once

#include "protocol/client/clientMessages.hpp"
#include "protocol/messages.hpp"
#include "protocol/server/serverMessages.hpp"

namespace client {
template <> struct PayloadTraits<HelloPayload> {
    static constexpr MessageType type = MessageType::HELLO;
    static constexpr size_t size = sizeof(HelloPayload);
    static constexpr size_t dataSize =
        size + constants::HEADER_SIZE - constants::HMAC_SIZE;
    static constexpr size_t hmacOffset =
        constants::HEADER_SIZE + offsetof(HelloPayload, hmac);
    static constexpr size_t msgSize = sizeof(Message<HelloPayload>);
};

template <> struct PayloadTraits<HeartBeatPayload> {
    static constexpr MessageType type = MessageType::HEARTBEAT;
    static constexpr size_t size = sizeof(HeartBeatPayload);
    static constexpr size_t dataSize =
        size + constants::HEADER_SIZE - constants::HMAC_SIZE;
    static constexpr size_t hmacOffset =
        constants::HEADER_SIZE + offsetof(HeartBeatPayload, hmac);
    static constexpr size_t msgSize = sizeof(Message<HeartBeatPayload>);
};

template <> struct PayloadTraits<LogoutPayload> {
    static constexpr MessageType type = MessageType::LOGOUT;
    static constexpr size_t size = sizeof(LogoutPayload);
    static constexpr size_t dataSize =
        size + constants::HEADER_SIZE - constants::HMAC_SIZE;
    static constexpr size_t hmacOffset =
        constants::HEADER_SIZE + offsetof(LogoutPayload, hmac);
    static constexpr size_t msgSize = sizeof(Message<LogoutPayload>);
};

template <> struct PayloadTraits<NewOrderPayload> {
    static constexpr MessageType type = MessageType::NEW_ORDER;
    static constexpr size_t size = sizeof(NewOrderPayload);
    static constexpr size_t dataSize =
        size + constants::HEADER_SIZE - constants::HMAC_SIZE;
    static constexpr size_t hmacOffset =
        constants::HEADER_SIZE + offsetof(NewOrderPayload, hmac);
    static constexpr size_t msgSize = sizeof(Message<NewOrderPayload>);
};

template <> struct PayloadTraits<CancelOrderPayload> {
    static constexpr MessageType type = MessageType::CANCEL_ORDER;
    static constexpr size_t size = sizeof(CancelOrderPayload);
    static constexpr size_t dataSize =
        size + constants::HEADER_SIZE - constants::HMAC_SIZE;
    static constexpr size_t hmacOffset =
        constants::HEADER_SIZE + offsetof(CancelOrderPayload, hmac);
    static constexpr size_t msgSize = sizeof(Message<CancelOrderPayload>);
};

template <> struct PayloadTraits<ModifyOrderPayload> {
    static constexpr MessageType type = MessageType::MODIFY_ORDER;
    static constexpr size_t size = sizeof(ModifyOrderPayload);
    static constexpr size_t dataSize =
        size + constants::HEADER_SIZE - constants::HMAC_SIZE;
    static constexpr size_t hmacOffset =
        constants::HEADER_SIZE + offsetof(ModifyOrderPayload, hmac);
    static constexpr size_t msgSize = sizeof(Message<ModifyOrderPayload>);
};

} // namespace client

namespace server {
template <> struct PayloadTraits<HelloAckPayload> {
    static constexpr MessageType type = MessageType::HELLO_ACK;
    static constexpr size_t size = sizeof(HelloAckPayload);
    static constexpr size_t dataSize =
        size + constants::HEADER_SIZE - constants::HMAC_SIZE;
    static constexpr size_t hmacOffset =
        constants::HEADER_SIZE + offsetof(HelloAckPayload, hmac);
    static constexpr size_t msgSize = sizeof(Message<HelloAckPayload>);
};

template <> struct PayloadTraits<LogoutAckPayload> {
    static constexpr MessageType type = MessageType::LOGOUT_ACK;
    static constexpr size_t size = sizeof(LogoutAckPayload);
    static constexpr size_t dataSize =
        size + constants::HEADER_SIZE - constants::HMAC_SIZE;
    static constexpr size_t hmacOffset =
        constants::HEADER_SIZE + offsetof(LogoutAckPayload, hmac);
    static constexpr size_t msgSize = sizeof(Message<LogoutAckPayload>);
};

template <> struct PayloadTraits<SessionTimeoutPayload> {
    static constexpr MessageType type = MessageType::SESSION_TIMEOUT;
    static constexpr size_t size = sizeof(SessionTimeoutPayload);
    static constexpr size_t dataSize =
        size + constants::HEADER_SIZE - constants::HMAC_SIZE;
    static constexpr size_t hmacOffset =
        constants::HEADER_SIZE + offsetof(SessionTimeoutPayload, hmac);
    static constexpr size_t msgSize = sizeof(Message<SessionTimeoutPayload>);
};

template <> struct PayloadTraits<OrderAckPayload> {
    static constexpr MessageType type = MessageType::ORDER_ACK;
    static constexpr size_t size = sizeof(OrderAckPayload);
    static constexpr size_t dataSize =
        size + constants::HEADER_SIZE - constants::HMAC_SIZE;
    static constexpr size_t hmacOffset =
        constants::HEADER_SIZE + offsetof(OrderAckPayload, hmac);
    static constexpr size_t msgSize = sizeof(Message<OrderAckPayload>);
};

template <> struct PayloadTraits<CancelAckPayload> {
    static constexpr MessageType type = MessageType::CANCEL_ACK;
    static constexpr size_t size = sizeof(CancelAckPayload);
    static constexpr size_t dataSize =
        size + constants::HEADER_SIZE - constants::HMAC_SIZE;
    static constexpr size_t hmacOffset =
        constants::HEADER_SIZE + offsetof(CancelAckPayload, hmac);
    static constexpr size_t msgSize = sizeof(Message<CancelAckPayload>);
};

template <> struct PayloadTraits<ModifyAckPayload> {
    static constexpr MessageType type = MessageType::MODIFY_ACK;
    static constexpr size_t size = sizeof(ModifyAckPayload);
    static constexpr size_t dataSize =
        size + constants::HEADER_SIZE - constants::HMAC_SIZE;
    static constexpr size_t hmacOffset =
        constants::HEADER_SIZE + offsetof(ModifyAckPayload, hmac);
    static constexpr size_t msgSize = sizeof(Message<ModifyAckPayload>);
};

template <> struct PayloadTraits<TradePayload> {
    static constexpr MessageType type = MessageType::MODIFY_ACK;
    static constexpr size_t size = sizeof(TradePayload);
    static constexpr size_t dataSize =
        size + constants::HEADER_SIZE - constants::HMAC_SIZE;
    static constexpr size_t hmacOffset =
        constants::HEADER_SIZE + offsetof(TradePayload, hmac);
    static constexpr size_t msgSize = sizeof(Message<TradePayload>);
};

} // namespace server
