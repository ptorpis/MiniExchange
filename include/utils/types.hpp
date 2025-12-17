#pragma once
#include <cstdint>
#include <ostream>
#include <vector>

using Price = std::int64_t;
using Qty = std::int64_t;

using OrderID = std::uint64_t;
using ClientID = std::uint64_t;
using Timestamp = std::uint64_t;
using TradeID = std::uint64_t;

using InstrumentID = std::uint32_t;

enum class OrderType : std::uint8_t { LIMIT, MARKET };
enum class OrderSide : std::uint8_t { BUY, SELL };
enum class TimeInForce : std::uint8_t {
    GOOD_TILL_CANCELLED,
    FILL_OR_KILL,
    END_OF_DAY,
    GOOD_TILL_DATE
};

enum class OrderStatus : uint8_t {
    NULLSTATUS = 0x00,
    NEW = 0x01,
    REJECTED = 0x02,
    PARTIALLY_FILLED = 0x03,
    FILLED = 0x04,
    CANCELLED = 0x05,
    MODIFIED = 0x06
};

inline std::ostream& operator<<(std::ostream& os, OrderStatus status) {
    switch (status) {
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

struct TradeEvent {
    TradeID tradeID;
    OrderID buyerOrderID;
    OrderID sellerOrderID;
    ClientID buyerID;
    ClientID sellerID;
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
    OrderStatus status;
    std::vector<TradeEvent> tradeVec;
    InstrumentID instrumentID;
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
