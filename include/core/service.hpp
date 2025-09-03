#pragma once
#include "core/order.hpp"
#include "protocol/messages.hpp"
#include "utils/types.hpp"
#include "utils/utils.hpp"
#include <chrono>
#include <limits>
#include <memory>

class OrderService {
public:
    OrderService() : idSqn_(0) {}

    static OrderRequest createRequestFromMessage(Message<NewOrderPayload>& msg) {

        OrderRequest req = OrderRequest{
            msg.payload.serverClientID,
            static_cast<OrderSide>(msg.payload.orderSide),
            static_cast<OrderType>(msg.payload.orderType),
            msg.payload.instrumentID,
            static_cast<Qty>(msg.payload.quantity),
            static_cast<Price>(msg.payload.price),
            static_cast<TimeInForce>(msg.payload.timeInForce),
            msg.payload.goodTillDate,
            // isvalid
            false,
        };

        req.valid = (msg.payload.price <= 0 || msg.payload.quantity <= 0) ? false : true;

        return req;
    }

    std::unique_ptr<Order> orderFromRequest(OrderRequest& req) {
        return std::make_unique<Order>(Order{++idSqn_, req.clientID, req.side, req.type,
                                             req.instrumentID, req.qty, req.price,
                                             req.tif, req.goodTill, OrderStatus::NEW,
                                             utils::getCurrentTimestampMicros()});
    }

private:
    OrderID idSqn_;
};