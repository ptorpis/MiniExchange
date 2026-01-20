#pragma once
#include "utils/utils.hpp"
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <ostream>
#include <type_traits>
#include <unordered_map>
#include <vector>

/**
 * @brief CRTP base class providing strong typing for primitive types.
 *
 * StrongType wraps a primitive type (int, float, etc.) to create distinct types that
 * prevent implicit conversions and accidental mixing of semantically different values.
 * Uses the Curiously Recurring Template Pattern (CRTP) to preserve the derived type
 * through operations.
 *
 * @tparam Derived The derived type (e.g., PriceTag, QtyTag) - CRTP pattern
 * @tparam Type The underlying primitive type to wrap (must be trivial)
 *
 * Features:
 * - Prevents mixing semantically different types (e.g., Price + Qty won't compile)
 * - Zero runtime overhead - all operations are constexpr and inline
 * - Arithmetic operators return the derived type, preserving strong typing
 * - Comparison operators support both strong types and raw values
 * - Stream output support via operator
 * - Hash support for use in unordered containers
 *
 * Example usage:
 * @code
 *   // Define tag types using CRTP
 *   struct PriceTag : StrongType<std::uint64_t, PriceTag> {
 *       using StrongType::StrongType;
 *   };
 *   struct QtyTag : StrongType<std::uint64_t, QtyTag> {
 *       using StrongType::StrongType;
 *   };
 *
 *   using Price = PriceTag;
 *   using Qty = QtyTag;
 *
 *   // Usage
 *   Price price{15000};
 *   Qty qty{100};
 *
 *   Price total = price + price;    // OK: Price + Price = Price
 *
 *   auto err = price + qty;          // ERROR: Can't mix Price and Qty
 *
 *   if (qty > 0) { ... }       // OK: Compare with raw value
 *
 *   std::uint64_t raw = price.value(); // Extract raw value when needed
 * @endcode
 *
 * @note Derived types must use CRTP: `struct MyType : StrongType<MyType, int>`
 * @note The underlying type must be trivial (verified by static_assert)
 */
template <typename Type, typename CRTP> struct StrongType {
    static_assert(std::is_trivial_v<Type>, "The member should be of trivial type");

    [[nodiscard]] constexpr auto value() const noexcept { return data_m; }

    explicit constexpr StrongType(Type value) noexcept : data_m(value) {}
    constexpr StrongType() noexcept : data_m{} {}

    constexpr auto operator<=>(const StrongType&) const noexcept = default;
    constexpr auto operator<=>(Type other) const noexcept { return data_m <=> other; }
    constexpr bool operator==(Type other) const noexcept { return data_m == other; }
    constexpr bool operator!() const noexcept { return data_m == 0; }
    constexpr bool operator==(const StrongType& other) const noexcept {
        return other.data_m == data_m;
    }
    constexpr bool operator!=(const StrongType& other) const noexcept {
        return other.data_m != data_m;
    }

    friend std::ostream& operator<<(std::ostream& os, const StrongType& st) {
        return os << st.data_m;
    }

    constexpr CRTP operator+(const CRTP& other) const noexcept {
        return CRTP{data_m + other.data_m};
    }

    constexpr CRTP operator-(const CRTP& other) const noexcept {
        return CRTP{data_m - other.data_m};
    }

    constexpr CRTP operator*(const CRTP& other) const noexcept {
        return CRTP{data_m * other.data_m};
    }

    constexpr CRTP operator/(const CRTP& other) const noexcept {
        return CRTP{data_m / other.data_m};
    }

    constexpr CRTP& operator+=(const CRTP& other) noexcept {
        data_m += other.data_m;
        return static_cast<CRTP&>(*this);
    }

    constexpr CRTP& operator-=(const CRTP& other) noexcept {
        data_m -= other.data_m;
        return static_cast<CRTP&>(*this);
    }

    constexpr CRTP& operator+=(Type value) noexcept {
        data_m += value;
        return static_cast<CRTP&>(*this);
    }

    constexpr CRTP& operator-=(Type value) noexcept {
        data_m -= value;
        return static_cast<CRTP&>(*this);
    }

    constexpr CRTP& operator++() noexcept {
        ++data_m;
        return static_cast<CRTP&>(*this);
    }

    constexpr CRTP operator++(int) noexcept {
        CRTP temp = static_cast<CRTP&>(*this);
        ++data_m;
        return temp;
    }

    constexpr CRTP& operator--() noexcept {
        --data_m;
        return static_cast<CRTP&>(*this);
    }

    constexpr CRTP operator--(int) noexcept {
        CRTP temp = static_cast<CRTP&>(*this);
        --data_m;
        return temp;
    }

protected:
    Type data_m;
};

// define tags like this then alias the desired name
struct PriceTag : StrongType<std::uint64_t, PriceTag> {
    using StrongType::StrongType;
};
struct QtyTag : StrongType<std::uint64_t, QtyTag> {
    using StrongType::StrongType;
};

struct OrderIDTag : StrongType<std::uint64_t, OrderIDTag> {
    using StrongType::StrongType;
};

struct ClientIDTag : StrongType<std::uint64_t, ClientIDTag> {
    using StrongType::StrongType;
};
struct ServerSqn32Tag : StrongType<std::uint32_t, ServerSqn32Tag> {
    using StrongType::StrongType;
};
struct ClientSqn32Tag : StrongType<std::uint32_t, ClientSqn32Tag> {
    using StrongType::StrongType;
};
struct InstrumentIDTag : StrongType<std::uint32_t, InstrumentIDTag> {
    using StrongType::StrongType;
};
struct TradeIDTag : StrongType<std::uint64_t, TradeIDTag> {
    using StrongType::StrongType;
};
struct ClientOrderIDTag : StrongType<std::uint64_t, ClientOrderIDTag> {
    using StrongType::StrongType;
};

struct MDSqnTag : StrongType<std::uint64_t, MDSqnTag> {
    using StrongType::StrongType;
};

namespace std {

/*
specializing hash functions for the strong types so map syntax becomes easier.

Instead of std::unordered_map<StrongType, *something*, StrongType::Hash>

it becomes -> std::unordered_map<StrongType, *something*> --> without the need of
specifying the hash function that is needed
*/

template <> struct hash<ClientIDTag> {
    hash() = default;
    hash(const hash&) = default;
    hash(hash&&) = default;
    hash& operator=(const hash&) = default;
    hash& operator=(hash&&) = default;

    std::size_t operator()(const ClientIDTag& key) const noexcept {
        return std::hash<std::uint64_t>{}(key.value());
    }
};

template <> struct hash<OrderIDTag> {
    hash() = default;
    hash(const hash&) = default;
    hash(hash&&) = default;
    hash& operator=(const hash&) = default;
    hash& operator=(hash&&) = default;

    std::size_t operator()(const OrderIDTag& key) const noexcept {
        return std::hash<std::uint64_t>{}(key.value());
    }
};

template <> struct hash<ClientOrderIDTag> {
    hash() = default;
    hash(const hash&) = default;
    hash(hash&&) = default;
    hash& operator=(const hash&) = default;
    hash& operator=(hash&&) = default;

    std::size_t operator()(const ClientOrderIDTag& key) const noexcept {
        return std::hash<std::uint64_t>{}(key.value());
    }
};

template <> struct hash<InstrumentIDTag> {
    hash() = default;
    hash(const hash&) = default;
    hash(hash&&) = default;
    hash& operator=(const hash&) = default;
    hash& operator=(hash&&) = default;

    std::size_t operator()(const InstrumentIDTag& key) const noexcept {
        return std::hash<std::uint32_t>{}(key.value());
    }
};
} // namespace std

using Price = PriceTag;
using Qty = QtyTag;
using OrderID = OrderIDTag;
using ClientOrderID = ClientOrderIDTag;
using ClientID = ClientIDTag;

// make sure that the server and client sequence numbers are never mixed
using ServerSqn32 = ServerSqn32Tag;
using ClientSqn32 = ClientSqn32Tag;

using Timestamp = std::uint64_t;
using TradeID = TradeIDTag;

using InstrumentID = InstrumentIDTag;

using MDSqn = MDSqnTag;

enum class OrderType : std::uint8_t { LIMIT = 0, MARKET };
enum class OrderSide : std::uint8_t { BUY = 0, SELL };
enum class TimeInForce : std::uint8_t {
    GOOD_TILL_CANCELLED,
    FILL_OR_KILL,
    END_OF_DAY,
    GOOD_TILL_DATE
};

enum class OrderStatus : uint8_t {
    PENDING = 0x00,
    NEW = 0x01,
    REJECTED = 0x02,
    PARTIALLY_FILLED = 0x03,
    FILLED = 0x04,
    CANCELLED = 0x05,
    MODIFIED = 0x06
};

struct Order {
    const OrderID orderID;             // 8 bytes
    const ClientID clientID;           // 8 bytes
    const ClientOrderID clientOrderID; // 8 bytes
    Qty qty;                           // 8 bytes
    const Price price;                 // 8 bytes
    const Timestamp goodTill;          // 8 bytes
    const Timestamp timestamp;         // 8 bytes
    const InstrumentID instrumentID;   // 4 bytes
    const TimeInForce tif;             // 1 byte
    const OrderSide side;              // 1 byte
    const OrderType type;              // 1 byte
    OrderStatus status;                // 1 byte
};

struct ClientOrder {
    ClientOrderID orderID;
    OrderID serverOrderID;
    InstrumentID instrumentID;
    OrderSide side;
    OrderType type;
    Price price;
    Qty originalQty;
    Qty remainingQty;
    OrderStatus status;
    TimeInForce tif;
    Timestamp goodTillDate;
    Timestamp submitTime;

    bool isPending() const { return status == OrderStatus::PENDING; }
    bool isOpen() const {
        return status == OrderStatus::NEW || status == OrderStatus::PARTIALLY_FILLED ||
               status == OrderStatus::MODIFIED;
    }
    bool isCancelled() const { return status == OrderStatus::CANCELLED; }
    bool isFilled() const { return status == OrderStatus::FILLED; }
};

struct Level2OrderBook {
    std::vector<std::pair<Price, Qty>> bids;
    std::vector<std::pair<Price, Qty>> asks;
};

using OrderQueue = std::deque<std::unique_ptr<Order>>;

struct Level3OrderBook {
    std::map<Price, OrderQueue, std::less<Price>> asks;
    std::map<Price, OrderQueue, std::greater<Price>> bids;
    std::unordered_map<OrderID, Order*> orderMap;
};

enum class BookUpdateEventType : std::uint8_t { ADD = 0, REDUCE = 1 };

inline std::ostream& operator<<(std::ostream& os, OrderStatus status) {
    switch (status) {
    case OrderStatus::PENDING:
        return os << "PENDING";
    case OrderStatus::NEW:
        return os << "NEW";
    case OrderStatus::CANCELLED:
        return os << "CANCELLED";
    case OrderStatus::MODIFIED:
        return os << "MODIFIED";
    case OrderStatus::FILLED:
        return os << "FILLED";
    case OrderStatus::PARTIALLY_FILLED:
        return os << "PARTIALLY_FILLED";
    case OrderStatus::REJECTED:
        return os << "REJECTED";
    default:
        return os << "UNKNOWN";
    }
}

enum class ModifyStatus : uint8_t {
    NULLSTATUS = 0x00,
    ACCEPTED = 0x01,
    INVALID = 0x02,
    NOT_FOUND = 0x03,
    NOT_AUTHENTICATED = 0x04,
    OUT_OF_ORDER = 0x05
};

inline std::ostream& operator<<(std::ostream& os, ModifyStatus status) {
    switch (status) {
    case ModifyStatus::ACCEPTED:
        return os << "ACCEPTED";
    case ModifyStatus::INVALID:
        return os << "INVALID";
    case ModifyStatus::NOT_FOUND:
        return os << "NOT_FOUND";
    case ModifyStatus::NOT_AUTHENTICATED:
        return os << "NOT_AUTHENTICATED";
    case ModifyStatus::OUT_OF_ORDER:
        return os << "OUT_OF_ORDER";
    default:
        return os << "UNKNOWN";
    }
}

enum class MessageType : std::uint8_t {
    HELLO = 0x01,
    HELLO_ACK = 0x02,
    LOGOUT = 0x03,
    LOGOUT_ACK = 0x04,
    NEW_ORDER = 0x0A,
    ORDER_ACK = 0x0B,
    CANCEL_ORDER = 0x0C,
    CANCEL_ACK = 0x0D,
    MODIFY_ORDER = 0x0E,
    MODIFY_ACK = 0x0F,
    TRADE = 0x10,
};

inline std::ostream& operator<<(std::ostream& os, MessageType type) {
    using enum MessageType;

    std::string_view name = [type]() -> std::string_view {
        switch (type) {
        case HELLO:
            return "HELLO";
        case HELLO_ACK:
            return "HELLO_ACK";
        case LOGOUT:
            return "LOGOUT";
        case LOGOUT_ACK:
            return "LOGOUT_ACK";
        case NEW_ORDER:
            return "NEW_ORDER";
        case ORDER_ACK:
            return "ORDER_ACK";
        case CANCEL_ORDER:
            return "CANCEL_ORDER";
        case CANCEL_ACK:
            return "CANCEL_ACK";
        case MODIFY_ORDER:
            return "MODIFY_ORDER";
        case MODIFY_ACK:
            return "MODIFY_ACK";
        case TRADE:
            return "TRADE";
        default:
            return "UNKNOWN";
        }
    }();

    return os << name << " (0x" << std::hex << std::uppercase << static_cast<int>(+type)
              << std::dec << ")";
}

struct TradeEvent {
    TradeID tradeID;
    OrderID buyerOrderID;
    OrderID sellerOrderID;
    ClientID buyerID;
    ClientID sellerID;
    ClientOrderID buyerClientOrderID;
    ClientOrderID sellerClientOrderID;
    Qty qty;
    Price price;
    Timestamp timestamp;
    InstrumentID instrumentID;
};

inline std::ostream& operator<<(std::ostream& os, const TradeEvent& e) {
    return os << "TRADE_EVENT|tradeID=" << e.tradeID << "|buyerOrderID=" << e.buyerOrderID
              << "|sellerOrderID=" << e.sellerOrderID << "|buyerID=" << e.buyerID
              << "|sellerID=" << e.sellerID << "|qty=" << e.qty << "|price=" << e.price
              << "|timestamp=" << e.timestamp << "|instrumentID=" << e.instrumentID
              << "\n";
}

struct MatchResult {
    OrderID orderID;
    Timestamp timestamp;
    Qty remainingQty;
    Price acceptedPrice;
    OrderStatus status;
    InstrumentID instrumentID;
    std::vector<TradeEvent> tradeVec;
};

inline std::ostream& operator<<(std::ostream& os, const MatchResult& m) {
    os << "MATCH_RESULT|orderID=" << m.orderID << "|timestamp" << m.timestamp
       << "|remainingQty=" << m.remainingQty << "|status=" << m.status
       << "|instrumentID=" << m.instrumentID << "\n";

    for (auto ev : m.tradeVec) {
        os << ev;
    }
    return os;
}

struct ModifyResult {
    ClientID serverClientID;
    OrderID oldOrderID;
    OrderID newOrderID;
    Qty newQty;
    Price newPrice;
    ModifyStatus status;
    InstrumentID instrumentID;

    std::optional<MatchResult> matchResult;
};

inline std::ostream& operator<<(std::ostream& os, const ModifyResult& m) {
    os << "MODIFY_RESULT|clientID=" << m.serverClientID << "|oldOrderID=" << m.oldOrderID
       << "|newOrderID=" << m.newOrderID << "|newQty=" << m.newQty
       << "|newPrice=" << m.newPrice << "|status=" << m.status
       << "|instrumentID=" << m.instrumentID << "\n";

    if (m.matchResult.has_value()) {
        os << m.matchResult.value();
    }
    return os;
}

inline std::ostream& operator<<(std::ostream& os, OrderType type) {
    switch (type) {
    case OrderType::LIMIT:
        return os << "LIMIT";
    case OrderType::MARKET:
        return os << "MARKET";
    default:
        return os << "UNKNOWN";
    }
}

inline std::ostream& operator<<(std::ostream& os, OrderSide type) {
    switch (type) {
    case OrderSide::BUY:
        return os << "BUY";
    case OrderSide::SELL:
        return os << "SELL";
    default:
        return os << "UNKNOWN";
    }
}

inline std::ostream& operator<<(std::ostream& os, TimeInForce type) {
    switch (type) {
    case TimeInForce::GOOD_TILL_CANCELLED:
        return os << "GOOD_TILL_CANCELLED";
    case TimeInForce::FILL_OR_KILL:
        return os << "FILL_OR_KILL";
    case TimeInForce::END_OF_DAY:
        return os << "END_OF_DAY";
    case TimeInForce::GOOD_TILL_DATE:
        return os << "GOOD_TILL_DATE";
    default:
        return os << "UNKNOWN";
    }
}
