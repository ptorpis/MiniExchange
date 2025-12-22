#pragma once

#include "utils/types.hpp"
#include <cstdint>

namespace client {
struct HelloPayload {
    std::uint8_t padding[8]{};
    template <typename F> void iterateElements([[maybe_unused]] F&& func) {}

    struct traits {
        static constexpr std::size_t payloadSize = 8;
        static constexpr MessageType type = MessageType::HELLO;
    };
};

struct LogoutPayload {
    ClientID serverClientID;
    template <typename F> void iterateElements(F&& func) { func(serverClientID); }

    struct traits {
        static constexpr std::size_t payloadSize = 8;
        static constexpr MessageType type = MessageType::LOGOUT;
    };
};

#pragma pack(push, 1)
struct NewOrderPayload {
    ClientID clientID;
    InstrumentID instrumentID;
    std::uint8_t orderSide;
    std::uint8_t orderType;
    std::uint8_t timeInForce;
    std::uint8_t padding{0};
    std::uint64_t qty;
    std::uint64_t price;
    Timestamp goodTillDate;

    template <typename F> void iterateElements(F&& func) {
        func(clientID);
        func(instrumentID);
        func(orderSide);
        func(orderType);
        func(timeInForce);
        func(padding);
        func(qty);
        func(price);
        func(goodTillDate);
    }

    struct traits {
        static constexpr std::size_t payloadSize = 40;
        static constexpr MessageType type = MessageType::NEW_ORDER;
    };
};
#pragma pack(pop)

#pragma pack(push, 1)
struct CancelOrderPayload {
    ClientID serverClientID;
    OrderID serverOrderID;
    InstrumentID instrumentID;
    std::uint8_t padding[4]{};

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(serverOrderID);
        func(instrumentID);
    }

    struct traits {
        static constexpr std::size_t payloadSize = 24;
        static constexpr MessageType type = MessageType::CANCEL_ORDER;
    };
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ModifyOrderPayload {
    ClientID serverClientID;
    OrderID serverOrderID;
    std::uint64_t newQty;
    std::uint64_t newPrice;
    InstrumentID instrumentID;
    std::uint8_t padding[4];

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(serverOrderID);
        func(newQty);
        func(newPrice);
        func(instrumentID);
    }

    struct traits {
        static constexpr std::size_t payloadSize = 40;
        static constexpr MessageType type = MessageType::MODIFY_ORDER;
    };
};
#pragma pack(pop)

static_assert(HelloPayload::traits::payloadSize == sizeof(HelloPayload));
static_assert(LogoutPayload::traits::payloadSize == sizeof(LogoutPayload));
static_assert(NewOrderPayload::traits::payloadSize == sizeof(NewOrderPayload));
static_assert(CancelOrderPayload::traits::payloadSize == sizeof(CancelOrderPayload));
static_assert(ModifyOrderPayload::traits::payloadSize == sizeof(ModifyOrderPayload));

static_assert(HelloPayload::traits::type == MessageType::HELLO);
static_assert(LogoutPayload::traits::type == MessageType::LOGOUT);
static_assert(NewOrderPayload::traits::type == MessageType::NEW_ORDER);
static_assert(CancelOrderPayload::traits::type == MessageType::CANCEL_ORDER);
static_assert(ModifyOrderPayload::traits::type == MessageType::MODIFY_ORDER);

} // namespace client
