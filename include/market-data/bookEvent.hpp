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
