#pragma once
#include <cstdint>
#include <vector>

using Price = int64_t;
using Qty = int64_t;

using ClientID = uint64_t;
using OrderID = uint64_t;
using InstrumentID = uint32_t;
using Timestamp = uint64_t;
using TradeID = uint64_t;

enum class OrderType : uint8_t { LIMIT, MARKET };
enum class OrderSide : uint8_t { BUY, SELL };
enum class OrderStatus : uint8_t { NEW, REJECTED, PARTIALLY_FILLED, FILLED, CANCELLED };
enum class TimeInForce : uint8_t { GTC };

struct OutboundMessage {
    int fd;
    std::vector<uint8_t> data;
};