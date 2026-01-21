#pragma once

#include "utils/types.hpp"

#include <cstdint>
#include <type_traits>

struct L2OrderBookUpdate {
    Price price;
    Qty amount;
    OrderSide side;
    BookUpdateEventType type;

    std::uint16_t _padding;
    std::uint32_t _padding2;
};

static_assert(std::is_trivially_copyable_v<L2OrderBookUpdate>);
static_assert(sizeof(L2OrderBookUpdate) == 24);

enum class L3EventType : std::uint8_t {
    ORDER_ADD_OR_INCREASE,
    ORDER_FILL_OR_REDUCE,
    ORDER_CANCELLED,
};

struct alignas(8) L3Update {
    Price price;                 // 8
    Qty qty;                     // 8
    OrderID orderID;             // 8
    ClientOrderID clientOrderID; // 8
    Timestamp timestamp;         // 8
    InstrumentID instrumentID;   // 4
    L3EventType eventType;       // 1
    OrderType orderType;         // 1
    OrderSide orderSide;         // 1
};

static_assert(sizeof(L3Update) == 48);
static_assert(std::is_trivially_copyable_v<L3Update>);
static_assert(std::is_standard_layout_v<L3Update>);
static_assert(alignof(L3Update) == 8);
