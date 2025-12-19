#pragma once

#include "utils/types.hpp"
#include <cstdint>

namespace client {
struct HelloPayload {
    std::uint8_t padding[8];
    template <typename F> void iterateElements(F&& func) { func(padding); }
};

struct LogoutPayload {
    ClientID serverClientID;
    template <typename F> void iterateElements(F&& func) { func(serverClientID); }
};

#pragma pack(push, 1)
struct NewOrderPayload {
    ClientID clientID;
    InstrumentID instrumentID;
    std::uint8_t orderSide;
    std::uint8_t orderType;
    std::uint8_t timeInForce;
    std::uint8_t padding{0};
    std::int64_t qty;
    std::int64_t price;
    Timestamp goodTillDate;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct CancelPayload {
    ClientID serverClientID;
    OrderID serverOrderID;
    InstrumentID instrumentID;
    std::uint8_t padding[4]{};

    template <typename F> void iterateElements(F&& func) {
        func(serverClientID);
        func(serverOrderID);
        func(instrumentID);
        func(padding);
    }
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ModifyOrderPayload {
    ClientID serverClientID;
    OrderID serverOrderID;
    std::int64_t newQty;
    std::int64_t newPrice;
    InstrumentID instrumentID;
    std::uint8_t padding[4];
};
#pragma pack(pop)

} // namespace client
