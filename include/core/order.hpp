#pragma once

#include "utils/types.hpp"
#include <iostream>
#include <ostream>

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

inline std::ostream& operator<<(std::ostream& os, const Order o) {
    return os << "orderID=" << o.orderID << "|clientID=" << o.clientID << "|qty=" << o.qty
              << "|price=" << o.price << "|goodTill=" << o.goodTill
              << "|timestamp=" << o.timestamp << "|instrumentID=" << o.instrumentID
              << "|timeInForce=" << o.tif << "|orderSide=" << o.side
              << "|orderType=" << o.type << "\n";
}
