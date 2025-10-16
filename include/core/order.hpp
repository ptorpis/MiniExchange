#pragma once
#include "protocol/statusCodes.hpp"
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
    statusCodes::OrderStatus status;
    const Timestamp timestamp;
    const uint32_t ref;
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
    const uint32_t ref;
};