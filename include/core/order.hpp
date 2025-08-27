#include "utils/types.hpp"

struct Order {
    const OrderID orderID;
    const ClientID clientID;
    const OrderSide side;
    const OrderType type;
    Qty qty;
    const Price price;
    TimeInForce tif;
    Timestamp goodTill;
    OrderStatus status;
    const Timestamp timestamp;
};
