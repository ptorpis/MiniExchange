#pragma once
#include "utils/types.hpp"

struct Order {
    const OrderID orderID;
    const ClientID clientID;
    const OrderSide side;
    const OrderType type;
    const InstrumentID instrumentID;
    Qty qty;
    const Price price;
    TimeInForce tif;
    Timestamp goodTill;
    OrderStatus status;
    const Timestamp timestamp;
};

struct OrderRequest {
    const ClientID clientID;
    const OrderSide side;
    const OrderType type;
    const InstrumentID instrumentID;
    Qty qty;
    const Price price;
    TimeInForce tif;
    Timestamp goodTill;
    bool valid;
};