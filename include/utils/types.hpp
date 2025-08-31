#pragma once
#include <cstdint>
#include <vector>

struct Price {
    int64_t value{0};

    constexpr Price() = default;
    explicit constexpr Price(int64_t v) : value(v) {}
    constexpr operator int64_t() const { return value; }

    constexpr bool operator==(const Price& other) { return value == other.value; }
    constexpr bool operator!=(const Price& other) { return value != other.value; }
    constexpr bool operator<(const Price& other) { return value < other.value; }
    constexpr bool operator<=(const Price& other) { return value <= other.value; }
    constexpr bool operator>(const Price& other) { return value > other.value; }
    constexpr bool operator>=(const Price& other) { return value >= other.value; }
};

struct Qty {
    int64_t value{0};

    constexpr Qty() = default;
    explicit constexpr Qty(int64_t v) : value(v) {}

    constexpr operator int64_t() const { return value; }
    constexpr bool operator==(const Qty& other) { return value == other.value; }
    constexpr bool operator!=(const Qty& other) { return value != other.value; }
    constexpr bool operator<(const Qty& other) { return value < other.value; }
    constexpr bool operator<=(const Qty& other) { return value <= other.value; }
    constexpr bool operator>(const Qty& other) { return value > other.value; }
    constexpr bool operator>=(const Qty& other) { return value >= other.value; }

    constexpr Qty operator+(const Qty& other) { return Qty(value + other.value); }
    constexpr Qty operator-(const Qty& other) { return Qty(value - other.value); }

    constexpr Qty& operator+=(const Qty& other) {
        value += other.value;
        return *this;
    }

    constexpr Qty& operator-=(const Qty& other) {
        value -= other.value;
        return *this;
    }
};

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