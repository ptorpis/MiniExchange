#pragma once

#include "utils/types.hpp"
#include <cstdint>

namespace client {

struct HelloPayload {
    std::uint8_t padding[8]{};

private:
    template <typename F, typename Self>
    static void iterateHelperWithNames(Self& self, F&& func) {
        func("padding", self.padding);
    }

public:
    template <typename F> void iterateElements(F&& func) {
        iterateHelperWithNames(*this, [&](auto&&, auto& field) { func(field); });
    }

    template <typename F> void iterateElements(F&& func) const {
        iterateHelperWithNames(*this, [&](auto&&, auto& field) { func(field); });
    }

    template <typename F> void iterateElementsWithNames(F&& func) {
        iterateHelperWithNames(*this, std::forward<F>(func));
    }

    template <typename F> void iterateElementsWithNames(F&& func) const {
        iterateHelperWithNames(*this, std::forward<F>(func));
    }

    struct traits {
        static constexpr std::size_t payloadSize = 8;
        static constexpr MessageType type = MessageType::HELLO;
    };
};

struct LogoutPayload {
    std::uint64_t serverClientID;

private:
    template <typename F, typename Self>
    static void iterateHelperWithNames(Self& self, F&& func) {
        func("serverClientID", self.serverClientID);
    }

public:
    template <typename F> void iterateElements(F&& func) {
        iterateHelperWithNames(*this, [&](auto&&, auto& field) { func(field); });
    }

    template <typename F> void iterateElements(F&& func) const {
        iterateHelperWithNames(*this, [&](auto&&, auto& field) { func(field); });
    }

    template <typename F> void iterateElementsWithNames(F&& func) {
        iterateHelperWithNames(*this, std::forward<F>(func));
    }

    template <typename F> void iterateElementsWithNames(F&& func) const {
        iterateHelperWithNames(*this, std::forward<F>(func));
    }

    struct traits {
        static constexpr std::size_t payloadSize = 8;
        static constexpr MessageType type = MessageType::LOGOUT;
    };
};

#pragma pack(push, 1)
struct NewOrderPayload {
    std::uint64_t serverClientID;
    std::uint64_t clientOrderID;
    std::uint32_t instrumentID;
    std::uint8_t orderSide;
    std::uint8_t orderType;
    std::uint8_t timeInForce;
    std::uint8_t padding{0};
    std::uint64_t qty;
    std::uint64_t price;
    std::uint64_t goodTillDate;

private:
    template <typename F, typename Self>
    static void iterateHelperWithNames(Self& self, F&& func) {
        func("serverClientID", self.serverClientID);
        func("clientOrderID", self.clientOrderID);
        func("instrumentID", self.instrumentID);
        func("orderSide", self.orderSide);
        func("orderType", self.orderType);
        func("timeInForce", self.timeInForce);
        func("padding", self.padding);
        func("qty", self.qty);
        func("price", self.price);
        func("goodTillDate", self.goodTillDate);
    }

public:
    template <typename F> void iterateElements(F&& func) {
        iterateHelperWithNames(*this, [&](auto&&, auto& field) { func(field); });
    }

    template <typename F> void iterateElements(F&& func) const {
        iterateHelperWithNames(*this, [&](auto&&, auto& field) { func(field); });
    }

    template <typename F> void iterateElementsWithNames(F&& func) {
        iterateHelperWithNames(*this, std::forward<F>(func));
    }

    template <typename F> void iterateElementsWithNames(F&& func) const {
        iterateHelperWithNames(*this, std::forward<F>(func));
    }

    struct traits {
        static constexpr std::size_t payloadSize = 48;
        static constexpr MessageType type = MessageType::NEW_ORDER;
    };
};
#pragma pack(pop)

#pragma pack(push, 1)
struct CancelOrderPayload {
    std::uint64_t serverClientID;
    std::uint64_t serverOrderID;
    std::uint64_t clientOrderID;
    std::uint32_t instrumentID;
    std::uint8_t padding[4]{};

private:
    template <typename F, typename Self>
    static void iterateHelperWithNames(Self& self, F&& func) {
        func("serverClientID", self.serverClientID);
        func("serverOrderID", self.serverOrderID);
        func("clientOrderID", self.clientOrderID);
        func("instrumentID", self.instrumentID);
        func("padding", self.padding);
    }

public:
    template <typename F> void iterateElements(F&& func) {
        iterateHelperWithNames(*this, [&](auto&&, auto& field) { func(field); });
    }

    template <typename F> void iterateElements(F&& func) const {
        iterateHelperWithNames(*this, [&](auto&&, auto& field) { func(field); });
    }

    template <typename F> void iterateElementsWithNames(F&& func) {
        iterateHelperWithNames(*this, std::forward<F>(func));
    }

    template <typename F> void iterateElementsWithNames(F&& func) const {
        iterateHelperWithNames(*this, std::forward<F>(func));
    }

    struct traits {
        static constexpr std::size_t payloadSize = 32;
        static constexpr MessageType type = MessageType::CANCEL_ORDER;
    };
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ModifyOrderPayload {
    std::uint64_t serverClientID;
    std::uint64_t serverOrderID;
    std::uint64_t clientOrderID;
    std::uint64_t newQty;
    std::uint64_t newPrice;
    std::uint32_t instrumentID;
    std::uint8_t padding[4]{};

private:
    template <typename F, typename Self>
    static void iterateHelperWithNames(Self& self, F&& func) {
        func("serverClientID", self.serverClientID);
        func("serverOrderID", self.serverOrderID);
        func("clientOrderID", self.clientOrderID);
        func("newQty", self.newQty);
        func("newPrice", self.newPrice);
        func("instrumentID", self.instrumentID);
        func("padding", self.padding);
    }

public:
    template <typename F> void iterateElements(F&& func) {
        iterateHelperWithNames(*this, [&](auto&&, auto& field) { func(field); });
    }

    template <typename F> void iterateElements(F&& func) const {
        iterateHelperWithNames(*this, [&](auto&&, auto& field) { func(field); });
    }

    template <typename F> void iterateElementsWithNames(F&& func) {
        iterateHelperWithNames(*this, std::forward<F>(func));
    }

    template <typename F> void iterateElementsWithNames(F&& func) const {
        iterateHelperWithNames(*this, std::forward<F>(func));
    }

    struct traits {
        static constexpr std::size_t payloadSize = 48;
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
