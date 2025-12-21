#pragma once
#include "utils/types.hpp"
#include <cstdint>

namespace server {

#pragma pack(push, 1)
struct HelloAckPayload {
    std::uint64_t serverClientID;
    std::uint8_t status;
    std::uint8_t padding[7];

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(status);
        func(padding);
    }

    struct traits {
        static constexpr std::size_t payloadSize = 16;
        static constexpr MessageType type = MessageType::HELLO_ACK;
    };
};
#pragma pack(pop)

#pragma pack(push, 1)
struct LogoutAckPayload {
    uint64_t serverClientID;
    uint8_t status;
    uint8_t padding[7];

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(status);
        func(padding);
    }

    struct traits {
        static constexpr std::size_t payloadSize = 16;
        static constexpr MessageType type = MessageType::LOGOUT_ACK;
    };
};
#pragma pack(pop)

#pragma pack(push, 1)
struct OrderAckPayload {
    uint64_t serverClientID;
    uint64_t serverOrderID;
    int64_t acceptedPrice;
    int64_t acceptedQty;
    uint64_t serverTime;
    uint32_t instrumentID;
    uint8_t status;
    uint8_t padding[3];

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(serverOrderID);
        func(acceptedPrice);
        func(acceptedQty);
        func(serverTime);
        func(instrumentID);
        func(status);
        func(padding);
    }

    struct traits {
        static constexpr std::size_t payloadSize = 48;
        static constexpr MessageType type = MessageType::ORDER_ACK;
    };
};
#pragma pack(pop)

#pragma pack(push, 1)
struct CancelAckPayload {
    uint64_t serverClientID;
    uint64_t serverOrderID;
    uint8_t status;
    uint8_t padding[7];

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(serverOrderID);
        func(status);
        func(padding);
    }

    struct traits {
        static constexpr std::size_t payloadSize = 24;
        static constexpr MessageType type = MessageType::CANCEL_ACK;
    };
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ModifyAckPayload {
    uint64_t serverClientID;
    uint64_t oldServerOrderID;
    uint64_t newServerOrderID;
    int64_t newQty;
    int64_t newPrice;
    uint8_t status;
    uint8_t padding[7];

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(oldServerOrderID);
        func(newServerOrderID);
        func(status);
        func(padding);
    }

    struct traits {
        static constexpr std::size_t payloadSize = 48;
        static constexpr MessageType type = MessageType::MODIFY_ACK;
    };
};
#pragma pack(pop)

#pragma pack(push, 1)
struct TradePayload {
    uint64_t serverClientID;
    uint64_t serverOrderID;
    uint64_t tradeID;
    int64_t filledQty;
    int64_t filledPrice;
    uint64_t timestamp;

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(serverOrderID);
        func(tradeID);
        func(filledQty);
        func(filledPrice);
        func(timestamp);
    }

    struct traits {
        static constexpr std::size_t payloadSize = 48;
        static constexpr MessageType type = MessageType::TRADE;
    };
};
#pragma pack(pop)

static_assert(HelloAckPayload::traits::payloadSize == sizeof(HelloAckPayload));
static_assert(LogoutAckPayload::traits::payloadSize == sizeof(LogoutAckPayload));
static_assert(OrderAckPayload::traits::payloadSize == sizeof(OrderAckPayload));
static_assert(CancelAckPayload::traits::payloadSize == sizeof(CancelAckPayload));
static_assert(ModifyAckPayload::traits::payloadSize == sizeof(ModifyAckPayload));
static_assert(TradePayload::traits::payloadSize == sizeof(TradePayload));

static_assert(HelloAckPayload::traits::type == ::MessageType::HELLO_ACK);
static_assert(LogoutAckPayload::traits::type == ::MessageType::LOGOUT_ACK);
static_assert(OrderAckPayload::traits::type == MessageType::ORDER_ACK);
static_assert(CancelAckPayload::traits::type == MessageType::CANCEL_ACK);
static_assert(ModifyAckPayload::traits::type == MessageType::MODIFY_ACK);
static_assert(TradePayload::traits::type == MessageType::TRADE);

} // namespace server
