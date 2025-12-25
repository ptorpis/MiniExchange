#pragma once
#include "utils/types.hpp"
#include <cstdint>

namespace server {

#pragma pack(push, 1)
struct HelloAckPayload {
    std::uint64_t serverClientID;
    std::uint8_t status;
    std::uint8_t padding[7]{};

private:
    template <typename F, typename Self>
    static void iterateHelperWithNames(Self& self, F&& func) {
        func("serverClientID", self.serverClientID);
        func("status", self.status);
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
        static constexpr std::size_t payloadSize = 16;
        static constexpr MessageType type = MessageType::HELLO_ACK;
    };
};
#pragma pack(pop)

#pragma pack(push, 1)
struct LogoutAckPayload {
    std::uint64_t serverClientID;
    std::uint8_t status;
    std::uint8_t padding[7]{};

private:
    template <typename F, typename Self>
    static void iterateHelperWithNames(Self& self, F&& func) {
        func("serverClientID", self.serverClientID);
        func("status", self.status);
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
        static constexpr std::size_t payloadSize = 16;
        static constexpr MessageType type = MessageType::LOGOUT_ACK;
    };
};
#pragma pack(pop)

#pragma pack(push, 1)
struct OrderAckPayload {
    std::uint64_t serverClientID;
    std::uint64_t serverOrderID;
    std::uint64_t clientOrderID;
    std::uint64_t acceptedPrice;
    std::uint64_t remainingQty;
    std::uint64_t serverTime;
    std::uint32_t instrumentID;
    std::uint8_t status;
    std::uint8_t padding[3]{};

private:
    template <typename F, typename Self>
    static void iterateHelperWithNames(Self& self, F&& func) {
        func("serverClientID", self.serverClientID);
        func("serverOrderID", self.serverOrderID);
        func("clientOrderID", self.clientOrderID);
        func("acceptedPrice", self.acceptedPrice);
        func("remainingQty", self.remainingQty);
        func("serverTime", self.serverTime);
        func("instrumentID", self.instrumentID);
        func("status", self.status);
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
        static constexpr std::size_t payloadSize = 56;
        static constexpr MessageType type = MessageType::ORDER_ACK;
    };
};
#pragma pack(pop)

#pragma pack(push, 1)
struct CancelAckPayload {
    std::uint64_t serverClientID;
    std::uint64_t serverOrderID;
    std::uint64_t clientOrderID;
    std::uint32_t instrumentID;
    std::uint8_t status;
    std::uint8_t padding[3]{};

private:
    template <typename F, typename Self>
    static void iterateHelperWithNames(Self& self, F&& func) {
        func("serverClientID", self.serverClientID);
        func("serverOrderID", self.serverOrderID);
        func("clientOrderID", self.clientOrderID);
        func("instrumentID", self.instrumentID);
        func("status", self.status);
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
        static constexpr MessageType type = MessageType::CANCEL_ACK;
    };
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ModifyAckPayload {
    std::uint64_t serverClientID;
    std::uint64_t oldServerOrderID;
    std::uint64_t newServerOrderID;
    std::uint64_t clientOrderID;
    std::uint64_t newQty;
    std::uint64_t newPrice;
    std::uint8_t status;
    std::uint8_t padding[7]{};

private:
    template <typename F, typename Self>
    static void iterateHelperWithNames(Self& self, F&& func) {
        func("serverClientID", self.serverClientID);
        func("oldServerOrderID", self.oldServerOrderID);
        func("newServerOrderID", self.newServerOrderID);
        func("clientOrderID", self.clientOrderID);
        func("newQty", self.newQty);
        func("newPrice", self.newPrice);
        func("status", self.status);
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
        static constexpr std::size_t payloadSize = 56;
        static constexpr MessageType type = MessageType::MODIFY_ACK;
    };
};
#pragma pack(pop)

#pragma pack(push, 1)
struct TradePayload {
    std::uint64_t serverClientID;
    std::uint64_t serverOrderID;
    std::uint64_t clientOrderID;
    std::uint64_t tradeID;
    std::uint64_t filledQty;
    std::uint64_t filledPrice;
    std::uint64_t timestamp;

private:
    template <typename F, typename Self>
    static void iterateHelperWithNames(Self& self, F&& func) {
        func("serverClientID", self.serverClientID);
        func("serverOrderID", self.serverOrderID);
        func("clientOrderID", self.clientOrderID);
        func("tradeID", self.tradeID);
        func("filledQty", self.filledQty);
        func("filledPrice", self.filledPrice);
        func("timestamp", self.timestamp);
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
        static constexpr std::size_t payloadSize = 56;
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

static_assert(HelloAckPayload::traits::type == MessageType::HELLO_ACK);
static_assert(LogoutAckPayload::traits::type == MessageType::LOGOUT_ACK);
static_assert(OrderAckPayload::traits::type == MessageType::ORDER_ACK);
static_assert(CancelAckPayload::traits::type == MessageType::CANCEL_ACK);
static_assert(ModifyAckPayload::traits::type == MessageType::MODIFY_ACK);
static_assert(TradePayload::traits::type == MessageType::TRADE);

} // namespace server
