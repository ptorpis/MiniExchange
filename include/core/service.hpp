#pragma once
#include "core/order.hpp"
#include "types.hpp"
#include <chrono>
#include <limits>

class OrderService {
public:
    OrderService() : idSqn_(0) {}

    Order createOrder(ClientID clientID, OrderSide side, OrderType type, Qty qty,
                      Price price, TimeInForce tif,
                      Timestamp goodTill = std::numeric_limits<Timestamp>::max(),
                      OrderStatus status) {

        return Order{++idSqn_, clientID, side, type, qty, price, tif, currentTimestamp_(),
                     status};
    }

private:
    OrderID idSqn_;
    static Timestamp currentTimestamp_() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }
};