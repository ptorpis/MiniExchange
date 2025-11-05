#pragma once
#include "protocol/messages.hpp"
#include "protocol/server/serverMessages.hpp"
#include <variant>

namespace client {

using IncomingMessageVariant =
    std::variant<server::HelloAckPayload, server::LogoutAckPayload,
                 server::OrderAckPayload, server::TradePayload, server::CancelAckPayload,
                 server::ModifyAckPayload>;

#pragma pack(push, 1)
struct HelloPayload {
    uint8_t apiKey[16];

    template <typename F> void iterateElements(F&& func) { func(apiKey); }

    std::array<uint8_t, 16> getApiKeyArray() const {
        std::array<uint8_t, 16> arr;
        std::copy(std::begin(apiKey), std::end(apiKey), arr.begin());
        return arr;
    }
};
#pragma pack(pop)

#pragma pack(push, 1)
struct HeartBeatPayload {
    uint64_t serverClientID;

    template <typename F> void iterateElements(F&& func) { func(serverClientID); }
};
#pragma pack(pop)

#pragma pack(push, 1)
struct LogoutPayload {
    uint64_t serverClientID;

    template <typename F> void iterateElements(F&& func) { func(serverClientID); }
};
#pragma pack(pop)

#pragma pack(push, 1)
struct NewOrderPayload {
    uint64_t serverClientID;
    uint32_t instrumentID;
    uint8_t orderSide;
    uint8_t orderType;
    uint8_t timeInForce;
    uint8_t padding{0};
    int64_t quantity;
    int64_t price;
    Timestamp goodTillDate;

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(instrumentID);
        func(orderSide);
        func(orderType);
        func(timeInForce);
        func(padding);
        func(quantity);
        func(price);
        func(goodTillDate);
    }
};
#pragma pack(pop)

#pragma pack(push, 1)
struct CancelOrderPayload {
    uint64_t serverClientID;
    uint64_t serverOrderID;

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(serverOrderID);
    }
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ModifyOrderPayload {
    uint64_t serverClientID;
    uint64_t serverOrderID;
    int64_t newQty;
    int64_t newPrice;

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(serverOrderID);
        func(newQty);
        func(newPrice);
    }
};
#pragma pack(pop)

} // namespace client