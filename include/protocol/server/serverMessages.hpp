#pragma once

#include "protocol/messages.hpp"

namespace server {
#pragma pack(push, 1)
struct HelloAckPayload {
    uint64_t serverClientID;
    uint8_t status;
    uint8_t padding[7];

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(status);
        func(padding);
    }
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
};
#pragma pack(pop)

#pragma pack(push, 1)
struct SessionTimeoutPayload {
    uint64_t serverClientID;
    uint64_t serverTime;

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(serverTime);
    }
};
#pragma pack(pop)

#pragma pack(push, 1)
struct OrderAckPayload {
    uint64_t serverClientID;
    uint32_t instrumentID;
    uint64_t serverOrderID;
    uint8_t status;
    int64_t acceptedPrice;
    int64_t acceptedQty;
    uint64_t serverTime;
    uint8_t padding[3];

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(instrumentID);
        func(serverOrderID);
        func(status);
        func(acceptedPrice);
        func(acceptedQty);
        func(serverTime);
        func(padding);
    }
};
#pragma pack(pop)

#pragma pack(push, 1)
struct CancelAckPayload {
    uint64_t serverClientID;
    uint64_t serverOrderID;
    uint8_t status;
    uint8_t padding[15];

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(serverOrderID);
        func(status);
        func(padding);
    }
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
};
#pragma pack(pop)

} // namespace server