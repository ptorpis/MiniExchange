#pragma once
#include "protocol/messages.hpp"
#include "protocol/statusCodes.hpp"
#include "utils/types.hpp"
#include <optional>

struct NewConnectionEvent {
    ClientID clientID;
    int fd;
    uint16_t port;
    std::array<uint8_t, 4> ip;
};

struct DisconnectEvent {
    ClientID clientID;
    int fd;
};

struct TradeEvent {
    TradeID tradeID;
    OrderID buyerOrderID;
    OrderID sellerOrderID;
    ClientID buyerID;
    ClientID sellerID;
    Qty qty;
    Price price;
    Timestamp timestamp;
    InstrumentID instrumentID{1};
};

struct OrderCancelledEvent {
    OrderID orderID;
};

struct AddedToBookEvent {
    OrderID orderID;
    ClientID clientID;
    OrderSide side;
    Qty qty;
    Price price;
    TimeInForce tif;
    Timestamp goodTill;
    InstrumentID instrumentID{1};
};

struct RemoveFromBookEvent {
    OrderID orderID;
};

struct ReceiveMessageEvent {
    int fd;
    ClientID clientID;
    uint8_t type;
};

struct SendMessageEvent {
    uint8_t type;
};

// engine internal
struct MatchResult {
    OrderID orderID;
    Timestamp ts;
    statusCodes::OrderStatus status;
    std::vector<TradeEvent> tradeVec;
    InstrumentID instrumentID{1};
};

// if the status is not accepted, the order IDs = 0
struct ModifyEvent {
    uint64_t serverClientID;
    uint64_t oldOrderID;
    uint64_t newOrderID;
    Qty newQty;
    Price newPrice;
    statusCodes::ModifyAckStatus status;
    InstrumentID instrumentID{1};
};

// engine internal
struct ModifyResult {
    ModifyEvent event;
    std::optional<MatchResult> result;
};

template <typename EventT> struct ServerEvent {
    Timestamp tsNs;
    EventT event;
};