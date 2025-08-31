#pragma once
#include "utils/types.hpp"

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
