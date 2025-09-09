#pragma once
#include "protocol/statusCodes.hpp"
#include "utils/types.hpp"
#include <optional>

struct TradeEvent {
    TradeID tradeID;
    OrderID buyerOrderID;
    OrderID sellerOrderID;
    ClientID buyerID;
    ClientID sellerID;
    Qty qty;
    Price price;
    Timestamp timestamp;
};

struct MatchResult {
    OrderID orderID;
    Timestamp ts;
    std::vector<TradeEvent> tradeVec;
};

// if the status is not accepcted, the order IDs = 0
struct ModifyEvent {
    uint64_t serverClientID;
    uint64_t oldOrderID;
    uint64_t newOrderID;
    statusCodes::ModifyStatus status;
};

struct ModifyResult {
    ModifyEvent event;
    std::optional<MatchResult> result;
};
