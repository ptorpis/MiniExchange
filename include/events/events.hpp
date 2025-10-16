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

    template <typename F> void iterateElements(F&& func) const {
        func("clientID", clientID);
        func("fd", fd);
        func("port", port);
        for (size_t i = 0; i < ip.size(); ++i) {
            func(("ip" + std::to_string(i)).c_str(), ip[i]);
        }
    }
};

struct DisconnectEvent {
    ClientID clientID;
    int fd;

    template <typename F> void iterateElements(F&& func) const {
        func("clientID", clientID);
        func("fd", fd);
    }
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

    template <typename F> void iterateElements(F&& func) const {
        func("tradeID", tradeID);
        func("buyerOrderID", buyerOrderID);
        func("sellerOrderID", sellerOrderID);
        func("buyerID", buyerID);
        func("sellerID", sellerID);
        func("qty", qty);
        func("price", price);
        func("timestamp", timestamp);
        func("instrumentID", instrumentID);
    }
};

struct OrderCancelledEvent {
    OrderID orderID;
    template <typename F> void iterateElements(F&& func) const {
        func("orderID", orderID);
    }
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
    uint32_t ref;

    template <typename F> void iterateElements(F&& func) const {
        func("orderID", orderID);
        func("clientID", clientID);
        func("side", side);
        func("qty", qty);
        func("price", price);
        func("tif", tif);
        func("goodTill", goodTill);
        func("instrumentID", instrumentID);
        func("ref", ref);
    }
};

struct RemoveFromBookEvent {
    OrderID orderID;
    template <typename F> void iterateElements(F&& func) const {
        func("orderID", orderID);
    }
};

struct ReceiveMessageEvent {
    int fd;
    ClientID clientID;
    uint8_t type;
    uint32_t ref;

    template <typename F> void iterateElements(F&& func) const {
        func("fd", fd);
        func("clientID", clientID);
        func("type", type);
        func("ref", ref);
    }
};

struct SendMessageEvent {
    uint8_t type;
    uint32_t ref;

    template <typename F> void iterateElements(F&& func) const {
        func("type", type);
        func("ref", ref);
    }
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

    template <typename F> void iterateElements(F&& func) const {
        func("serverClientID", serverClientID);
        func("oldOrderID", oldOrderID);
        func("newOrderID", newOrderID);
        func("newQty", newQty);
        func("newPrice", newPrice);
        func("status", status);
        func("instrumentID", instrumentID);
    }
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