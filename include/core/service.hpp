#pragma once
#include "core/order.hpp"
#include "protocol/messages.hpp"
#include "utils/types.hpp"
#include <chrono>
#include <limits>

class OrderService {
public:
    OrderService() : idSqn_(0) {}

    std::optional<Order> createOrderFromMessage(Message<NewOrderPayload>& msg) {
        NewOrderPayload pyld = msg.payload;
        if (pyld.price <= 0 || pyld.quantity <= 0) {
            return std::nullopt;
        }
        return Order{
            ++idSqn_,
            pyld.serverClientID,
            static_cast<OrderSide>(pyld.orderSide),
            static_cast<OrderType>(pyld.orderType),
            pyld.instrumentID,
            static_cast<Qty>(pyld.quantity),
            static_cast<Price>(pyld.price),
            static_cast<TimeInForce>(pyld.timeInForce),
            std::numeric_limits<Timestamp>::max(),
            OrderStatus::NEW,
            currentTimestamp_(),
        };
    }

private:
    OrderID idSqn_;
    static Timestamp currentTimestamp_() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }
};