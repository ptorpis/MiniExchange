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
    uint8_t hmac[32];

    template <typename F> void iterateElements(F&& func) {
        func(apiKey);
        func(hmac);
    }

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
    uint8_t padding[8];

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(padding);
    }
};
#pragma pack(pop)

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

#pragma pack(push, 1)
struct ModifyOrderPayload {
    uint64_t serverClientID;
    uint64_t serverOrderID;
    int64_t newQty;
    int64_t newPrice;
    uint8_t hmac[32];

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(serverOrderID);
        func(newQty);
        func(newPrice);
        func(hmac);
    }
};
#pragma pack(pop)

} // namespace client