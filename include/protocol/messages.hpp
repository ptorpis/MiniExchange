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
} // namespace constants

// only specialized versions exist which contain the traits of each message
template <typename T> struct PayloadTraits;

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

#pragma pack(push, 1)
struct HelloPayload {
    uint8_t apiKey[16];
    uint8_t hmac[32];

    template <typename F> void iterateElements(F&& func) {
        func(apiKey);
        func(hmac);
    }
};
#pragma pack(pop)

template <> struct PayloadTraits<HelloPayload> {
    static constexpr MessageType type = MessageType::HELLO;
    static constexpr size_t size = sizeof(HelloPayload);
    static constexpr size_t dataSize =
        size + constants::HEADER_SIZE - constants::HMAC_SIZE;
    static constexpr size_t hmacOffset =
        constants::HEADER_SIZE + offsetof(HelloPayload, hmac);
};

#pragma pack(push, 1)
struct HelloAckPayload {
    uint64_t serverClientID;
    uint8_t status;
    uint8_t padding[7];
    uint8_t hmac[32];

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(status);
        func(padding);
        func(hmac);
    }
};
#pragma pack(pop)

template <> struct PayloadTraits<HelloAckPayload> {
    static constexpr MessageType type = MessageType::HELLO_ACK;
    static constexpr size_t size = sizeof(HelloAckPayload);
    static constexpr size_t dataSize =
        size + constants::HEADER_SIZE - constants::HMAC_SIZE;
    static constexpr size_t hmacOffset =
        constants::HEADER_SIZE + offsetof(HelloAckPayload, hmac);
};

#pragma pack(push, 1)
struct HeartBeatPayload {
    uint64_t serverClientID;
    uint8_t padding[8];
    uint8_t hmac[32];

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(padding);
        func(hmac);
    }
};
#pragma pack(pop)

template <> struct PayloadTraits<HeartBeatPayload> {
    static constexpr MessageType type = MessageType::HEARTBEAT;
    static constexpr size_t size = sizeof(HeartBeatPayload);
    static constexpr size_t dataSize =
        size + constants::HEADER_SIZE - constants::HMAC_SIZE;
    static constexpr size_t hmacOffset =
        constants::HEADER_SIZE + offsetof(HeartBeatPayload, hmac);
};

#pragma pack(push, 1)
struct LogoutPayload {
    uint64_t serverClientID;
    uint8_t padding[8];
    uint8_t hmac[32];

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(padding);
        func(hmac);
    }
};
#pragma pack(pop)

template <> struct PayloadTraits<LogoutPayload> {
    static constexpr MessageType type = MessageType::LOGOUT;
    static constexpr size_t size = sizeof(LogoutPayload);
    static constexpr size_t dataSize =
        size + constants::HEADER_SIZE - constants::HMAC_SIZE;
    static constexpr size_t hmacOffset =
        constants::HEADER_SIZE + offsetof(LogoutPayload, hmac);
};

#pragma pack(push, 1)
struct LogoutAckPayload {
    uint64_t serverClientID;
    uint8_t status;
    uint8_t padding[7];
    uint8_t hmac[32];

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(status);
        func(padding);
        func(hmac);
    }
};
#pragma pack(pop)

template <> struct PayloadTraits<LogoutAckPayload> {
    static constexpr MessageType type = MessageType::LOGOUT_ACK;
    static constexpr size_t size = sizeof(LogoutAckPayload);
    static constexpr size_t dataSize =
        size + constants::HEADER_SIZE - constants::HMAC_SIZE;
    static constexpr size_t hmacOffset =
        constants::HEADER_SIZE + offsetof(LogoutAckPayload, hmac);
};

#pragma pack(push, 1)
struct SessionTimeoutPayload {
    uint64_t serverClientID;
    uint64_t serverTime;
    uint8_t hmac[32];

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(serverTime);
        func(hmac);
    }
};
#pragma pack(pop)

template <> struct PayloadTraits<SessionTimeoutPayload> {
    static constexpr MessageType type = MessageType::SESSION_TIMEOUT;
    static constexpr size_t size = sizeof(SessionTimeoutPayload);
    static constexpr size_t dataSize =
        size + constants::HEADER_SIZE - constants::HMAC_SIZE;
    static constexpr size_t hmacOffset =
        constants::HEADER_SIZE + offsetof(SessionTimeoutPayload, hmac);
};

#pragma pack(push, 1)
struct NewOrderPayload {
    uint64_t serverClientID;
    uint32_t instrumentID;
    uint8_t orderSide;
    uint8_t orderType;
    int64_t quantity;
    int64_t price;
    uint8_t timeInForce;
    uint64_t goodTillDate;
    uint8_t padding[9];
    uint8_t hmac[32];

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(instrumentID);
        func(orderSide);
        func(orderType);
        func(quantity);
        func(price);
        func(timeInForce);
        func(goodTillDate);
        func(padding);
        func(hmac);
    }
};
#pragma pack(pop)

template <> struct PayloadTraits<NewOrderPayload> {
    static constexpr MessageType type = MessageType::NEW_ORDER;
    static constexpr size_t size = sizeof(NewOrderPayload);
    static constexpr size_t dataSize =
        size + constants::HEADER_SIZE - constants::HMAC_SIZE;
    static constexpr size_t hmacOffset =
        constants::HEADER_SIZE + offsetof(NewOrderPayload, hmac);
};

#pragma pack(push, 1)
struct OrderAckPayload {
    uint64_t serverClientID;
    uint32_t instrumentID;
    uint64_t serverOrderID;
    uint8_t status;
    int64_t acceptedPrice;
    uint64_t serverTime;
    uint32_t latency;
    uint8_t padding[7];
    uint8_t hmac[32];

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(instrumentID);
        func(serverOrderID);
        func(status);
        func(acceptedPrice);
        func(serverTime);
        func(latency);
        func(padding);
        func(hmac);
    }
};
#pragma pack(pop)

template <> struct PayloadTraits<OrderAckPayload> {
    static constexpr MessageType type = MessageType::ORDER_ACK;
    static constexpr size_t size = sizeof(OrderAckPayload);
    static constexpr size_t dataSize =
        size + constants::HEADER_SIZE - constants::HMAC_SIZE;
    static constexpr size_t hmacOffset =
        constants::HEADER_SIZE + offsetof(OrderAckPayload, hmac);
};

#pragma pack(push, 1)
struct CancelOrderPayload {
    uint64_t serverClientID;
    uint64_t serverOrderID;
    uint8_t padding[16];
    uint8_t hmac[32];

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(serverOrderID);
        func(padding);
        func(hmac);
    }
};
#pragma pack(pop)

template <> struct PayloadTraits<CancelOrderPayload> {
    static constexpr MessageType type = MessageType::CANCEL_ORDER;
    static constexpr size_t size = sizeof(CancelOrderPayload);
    static constexpr size_t dataSize =
        size + constants::HEADER_SIZE - constants::HMAC_SIZE;
    static constexpr size_t hmacOffset =
        constants::HEADER_SIZE + offsetof(CancelOrderPayload, hmac);
};

#pragma pack(push, 1)
struct CancelAckPayload {
    uint64_t serverClientID;
    uint64_t serverOrderID;
    uint8_t status;
    uint8_t padding[15];
    uint8_t hmac[32];

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(serverOrderID);
        func(status);
        func(padding);
        func(hmac);
    }
};
#pragma pack(pop)

template <> struct PayloadTraits<CancelAckPayload> {
    static constexpr MessageType type = MessageType::CANCEL_ACK;
    static constexpr size_t size = sizeof(CancelAckPayload);
    static constexpr size_t dataSize =
        size + constants::HEADER_SIZE - constants::HMAC_SIZE;
    static constexpr size_t hmacOffset =
        constants::HEADER_SIZE + offsetof(CancelAckPayload, hmac);
};

#pragma pack(push, 1)
struct ModifyOrderPayload {
    uint64_t serverClientID;
    uint64_t serverOrderID;
    int64_t newQuantity;
    int64_t newPrice;
    uint8_t hmac[32];

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(serverOrderID);
        func(newQuantity);
        func(newPrice);
        func(hmac);
    }
};
#pragma pack(pop)

template <> struct PayloadTraits<ModifyOrderPayload> {
    static constexpr MessageType type = MessageType::MODIFY_ORDER;
    static constexpr size_t size = sizeof(ModifyOrderPayload);
    static constexpr size_t dataSize =
        size + constants::HEADER_SIZE - constants::HMAC_SIZE;
    static constexpr size_t hmacOffset =
        constants::HEADER_SIZE + offsetof(ModifyOrderPayload, hmac);
};

#pragma pack(push, 1)
struct ModifyAckPayload {
    uint64_t serverClientID;
    uint64_t oldServerOrderID;
    uint64_t newServerOrderID;
    uint8_t status;
    uint8_t padding[7];
    uint8_t hmac[32];

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(oldServerOrderID);
        func(newServerOrderID);
        func(status);
        func(padding);
        func(hmac);
    }
};
#pragma pack(pop)

template <> struct PayloadTraits<ModifyAckPayload> {
    static constexpr MessageType type = MessageType::MODIFY_ACK;
    static constexpr size_t size = sizeof(ModifyAckPayload);
    static constexpr size_t dataSize =
        size + constants::HEADER_SIZE - constants::HMAC_SIZE;
    static constexpr size_t hmacOffset =
        constants::HEADER_SIZE + offsetof(ModifyAckPayload, hmac);
};

#pragma pack(push, 1)
struct TradePayload {
    uint64_t serverClientID;
    uint64_t serverOrderID;
    uint64_t tradeID;
    int64_t filledQty;
    int64_t filledPrice;
    uint64_t timestamp;
    uint8_t hmac[32];

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(serverOrderID);
        func(tradeID);
        func(filledQty);
        func(filledPrice);
        func(timestamp);
        func(hmac);
    }
};
#pragma pack(pop)

template <> struct PayloadTraits<TradePayload> {
    static constexpr MessageType type = MessageType::MODIFY_ACK;
    static constexpr size_t size = sizeof(TradePayload);
    static constexpr size_t dataSize =
        size + constants::HEADER_SIZE - constants::HMAC_SIZE;
    static constexpr size_t hmacOffset =
        constants::HEADER_SIZE + offsetof(TradePayload, hmac);
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

template <typename Payload> struct Message {
    MessageHeader header;
    Payload payload;
};

namespace constants {
enum class HeaderFlags : uint8_t { PROTOCOL_VERSION = 0x01 };
}

template <typename Payload> inline MessageHeader makeHeader(Session& session) {
    MessageHeader header{};
    header.messageType = std::to_underlying(PayloadTraits<Payload>::type);
    header.protocolVersionFlag =
        std::to_underlying(constants::HeaderFlags::PROTOCOL_VERSION);
    header.payLoadLength = static_cast<uint16_t>(PayloadTraits<Payload>::size);
    header.clientMsgSqn = session.clientSqn;
    header.serverMsgSqn = ++session.serverSqn;
    std::memset(header.reservedFlags, 0, sizeof(header.reservedFlags));
    std::memset(header.padding, 0, sizeof(header.padding));

    return header;
}

template <typename Payload>
inline MessageHeader makeClientHeader(ClientSession& session) {
    MessageHeader header{};
    header.messageType = std::to_underlying(PayloadTraits<Payload>::type);
    header.protocolVersionFlag =
        std::to_underlying(constants::HeaderFlags::PROTOCOL_VERSION);
    header.payLoadLength = static_cast<uint16_t>(PayloadTraits<Payload>::size);
    header.clientMsgSqn = ++session.clientSqn;
    header.serverMsgSqn = session.serverSqn;
    std::memset(header.reservedFlags, 0, sizeof(header.reservedFlags));
    std::memset(header.padding, 0, sizeof(header.padding));

    return header;
}

struct MessageFactory {
    static Message<HelloAckPayload> makeHelloAck(Session& session,
                                                 statusCodes::HelloStatus status) {
        Message<HelloAckPayload> msg;

        msg.header = makeHeader<HelloAckPayload>(session);

        msg.payload.serverClientID = session.serverClientID;
        msg.payload.status = std::to_underlying(status);

        std::fill(std::begin(msg.payload.hmac), std::end(msg.payload.hmac), 0x00);
        std::fill(std::begin(msg.payload.padding), std::end(msg.payload.padding), 0x00);

        return msg;
    }

    static Message<LogoutAckPayload> makeLoutAck(Session& session,
                                                 statusCodes::LogoutStatus status) {
        Message<LogoutAckPayload> msg;

        msg.header = makeHeader<LogoutAckPayload>(session);

        msg.payload.serverClientID = session.serverClientID;
        msg.payload.status = std::to_underlying(status);

        std::fill(std::begin(msg.payload.hmac), std::end(msg.payload.hmac), 0x00);
        std::fill(std::begin(msg.payload.padding), std::end(msg.payload.padding), 0x00);

        return msg;
    }

    static Message<OrderAckPayload> makeOrderAck(Session& session, OrderRequest& req,
                                                 std::optional<OrderID> orderID,
                                                 statusCodes::OrderAckStatus status,
                                                 Timestamp ts) {

        Message<OrderAckPayload> ack;
        Timestamp currentTime = utils::getCurrentTimestampMicros();
        ack.header = makeHeader<OrderAckPayload>(session);
        ack.payload.serverClientID = session.serverClientID;
        ack.payload.instrumentID = req.instrumentID;
        ack.payload.status = std::to_underlying(status);
        ack.payload.serverTime = currentTime;
        ack.payload.latency = static_cast<int32_t>(currentTime - ts);
        if (!orderID) {
            ack.payload.serverOrderID = 0x00;
            ack.payload.acceptedPrice = 0;
        } else {
            ack.payload.serverOrderID = orderID.value();
            ack.payload.acceptedPrice = req.price;
        }

        std::fill(std::begin(ack.payload.hmac), std::end(ack.payload.hmac), 0x00);
        std::fill(std::begin(ack.payload.padding), std::end(ack.payload.padding), 0x00);
        return ack;
    }

    static Message<TradePayload> makeTradeMsg(Session& session, TradeEvent& trade,
                                              bool isBuyer) {
        Message<TradePayload> msg;
        msg.header = makeHeader<TradePayload>(session);

        msg.payload.tradeID = session.getNextExeID();
        msg.payload.filledQty = trade.qty;
        msg.payload.filledPrice = trade.price;
        msg.payload.timestamp = utils::getCurrentTimestampMicros();

        if (isBuyer) {
            msg.payload.serverClientID = trade.buyerID;
            msg.payload.serverOrderID = trade.buyerOrderID;
        } else {
            msg.payload.serverClientID = trade.sellerID;
            msg.payload.serverOrderID = trade.sellerOrderID;
        }

        std::fill(std::begin(msg.payload.hmac), std::end(msg.payload.hmac), 0x00);

        return msg;
    }

    static Message<CancelAckPayload> makeCancelAck(Session& session,
                                                   const OrderID orderID,
                                                   statusCodes::CancelAckStatus status) {
        Message<CancelAckPayload> msg;
        msg.header = makeHeader<CancelAckPayload>(session);
        msg.payload.serverClientID = session.serverClientID;
        msg.payload.serverOrderID = orderID;
        msg.payload.status = std::to_underlying(status);

        std::fill(std::begin(msg.payload.padding), std::end(msg.payload.padding), 0x00);
        std::fill(std::begin(msg.payload.hmac), std::end(msg.payload.hmac), 0x00);

        return msg;
    }

    static Message<ModifyAckPayload> makeModifyAck(Session& session, OrderID oldOrderID,
                                                   OrderID newOrderID,
                                                   statusCodes::ModifyStatus status) {
        Message<ModifyAckPayload> msg;
        msg.header = makeHeader<ModifyAckPayload>(session);
        msg.payload.oldServerOrderID = oldOrderID;
        msg.payload.newServerOrderID = newOrderID;
        msg.payload.status = std::to_underlying(status);

        std::fill(std::begin(msg.payload.padding), std::end(msg.payload.padding), 0x00);
        std::fill(std::begin(msg.payload.hmac), std::end(msg.payload.hmac), 0x00);

        return msg;
    }
};

namespace constants {

namespace PayloadSize {
inline constexpr size_t HELLO = sizeof(HelloPayload);
inline constexpr size_t HELLO_ACK = sizeof(HelloAckPayload);
inline constexpr size_t HEARTBEAT = sizeof(HeartBeatPayload);
inline constexpr size_t LOGOUT = sizeof(LogoutPayload);
inline constexpr size_t LOGOUT_ACK = sizeof(LogoutAckPayload);
inline constexpr size_t SESSION_TIMEOUT = sizeof(SessionTimeoutPayload);
inline constexpr size_t NEW_ORDER = sizeof(NewOrderPayload);
inline constexpr size_t ORDER_ACK = sizeof(OrderAckPayload);
inline constexpr size_t CANCEL_ORDER = sizeof(CancelOrderPayload);
inline constexpr size_t CANCEL_ACK = sizeof(CancelAckPayload);
inline constexpr size_t MODIFY_ORDER = sizeof(ModifyOrderPayload);
inline constexpr size_t MODIFY_ACK = sizeof(ModifyAckPayload);
inline constexpr size_t TRADE = sizeof(TradePayload);
} // namespace PayloadSize

// header + payload - HMAC
namespace DataSize {
inline constexpr size_t HELLO = HEADER_SIZE + PayloadSize::HELLO - HMAC_SIZE;
inline constexpr size_t HELLO_ACK = HEADER_SIZE + PayloadSize::HELLO_ACK - HMAC_SIZE;
inline constexpr size_t HEARTBEAT = HEADER_SIZE + PayloadSize::HEARTBEAT - HMAC_SIZE;
inline constexpr size_t LOGOUT = HEADER_SIZE + PayloadSize::LOGOUT - HMAC_SIZE;
inline constexpr size_t LOGOUT_ACK = HEADER_SIZE + PayloadSize::LOGOUT_ACK - HMAC_SIZE;
inline constexpr size_t SESSION_TIMEOUT =
    HEADER_SIZE + PayloadSize::SESSION_TIMEOUT - HMAC_SIZE;
inline constexpr size_t NEW_ORDER = HEADER_SIZE + PayloadSize::NEW_ORDER - HMAC_SIZE;
inline constexpr size_t ORDER_ACK = HEADER_SIZE + PayloadSize::ORDER_ACK - HMAC_SIZE;
inline constexpr size_t CANCEL_ORDER =
    HEADER_SIZE + PayloadSize::CANCEL_ORDER - HMAC_SIZE;
inline constexpr size_t CANCEL_ACK = HEADER_SIZE + PayloadSize::CANCEL_ACK - HMAC_SIZE;
inline constexpr size_t MODIFY_ORDER =
    HEADER_SIZE + PayloadSize::MODIFY_ORDER - HMAC_SIZE;
inline constexpr size_t MODIFY_ACK = HEADER_SIZE + PayloadSize::MODIFY_ACK - HMAC_SIZE;
inline constexpr size_t TRADE = HEADER_SIZE + PayloadSize::TRADE - HMAC_SIZE;
} // namespace DataSize

namespace HMACOffset {
constexpr size_t HELLO_OFFSET = HEADER_SIZE + offsetof(HelloPayload, hmac);
constexpr size_t LOGOUT_OFFSET = HEADER_SIZE + offsetof(LogoutPayload, hmac);
constexpr size_t CANCEL_OFFSET = HEADER_SIZE + offsetof(CancelOrderPayload, hmac);
constexpr size_t MODIFY_OFFSET = HEADER_SIZE + offsetof(ModifyOrderPayload, hmac);
} // namespace HMACOffset

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
static_assert(sizeof(MessageHeader) == 16, "MessageHeader size incorrect");
static_assert(sizeof(HelloPayload) == 48, "HelloPayload size incorrect");
static_assert(sizeof(HelloAckPayload) == 48, "HelloAckPayload size incorrect");
static_assert(sizeof(HeartBeatPayload) == 48, "HeartBeatPayload size incorrect");
static_assert(sizeof(LogoutPayload) == 48, "LogoutPayload size incorrect");
static_assert(sizeof(LogoutAckPayload) == 48, "LogoutAckPayload size incorrect");
static_assert(sizeof(SessionTimeoutPayload) == 48,
              "SessionTimeoutPayload size incorrect");
static_assert(sizeof(NewOrderPayload) == 80, "NewOrderPayload size incorrect");
static_assert(sizeof(OrderAckPayload) == 80, "OrderAckPayload size incorrect");
static_assert(sizeof(CancelOrderPayload) == 64, "CancelOrderPayload size incorrect");
static_assert(sizeof(CancelAckPayload) == 64, "CancelAckPayload size incorrect");
static_assert(sizeof(ModifyOrderPayload) == 64, "ModifyOrderPayload size incorrect");
static_assert(sizeof(ModifyAckPayload) == 64, "ModifyAckPayload size incorrect");
static_assert(sizeof(TradePayload) == 80, "TradePayload size incorrect");
static_assert(sizeof(ErrorMessagePayload) == 48, "ErrorMessagePayload size incorrect");