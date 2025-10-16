/*
To keep the responsibility to create request objects and order objects, this class
will do that instead of the engine.

The createRequestFromMessage() function will validate input, the orderFromRequest
assumes that the former validation was sufficient, hence it assumes correct input.

The createRequestFromMessage() function is used by the middle layers,
the orderFromRequest() and orderModified() are for the engine.
*/

#pragma once
#include "core/order.hpp"
#include "protocol/client/clientMessages.hpp"
#include "protocol/messages.hpp"
#include "utils/types.hpp"
#include "utils/utils.hpp"
#include <chrono>
#include <limits>
#include <memory>

class OrderService {
public:
    OrderService() : idSqn_(0) {}

    static OrderRequest createRequestFromMessage(Message<client::NewOrderPayload>& msg) {

        OrderRequest req = OrderRequest{
            msg.payload.serverClientID, static_cast<OrderSide>(msg.payload.orderSide),
            static_cast<OrderType>(msg.payload.orderType), msg.payload.instrumentID,
            static_cast<Qty>(msg.payload.quantity), static_cast<Price>(msg.payload.price),
            static_cast<TimeInForce>(msg.payload.timeInForce), msg.payload.goodTillDate,
            // isvalid
            false, msg.header.clientMsgSqn};

        if (msg.payload.orderType == +OrderType::LIMIT) {
            req.valid =
                (msg.payload.price <= 0 || msg.payload.quantity <= 0) ? false : true;
        } else {
            req.valid =
                (msg.payload.price == 0 && msg.payload.quantity > 0) ? true : false;
        }

        return req;
    }

    std::unique_ptr<Order> orderFromRequest(const OrderRequest& req) {
        return std::make_unique<Order>(
            Order{++idSqn_, req.clientID, req.side, req.type, req.instrumentID, req.qty,
                  req.price, req.tif, req.goodTill, statusCodes::OrderStatus::NEW,
                  utils::getTimestampNs(), req.ref});
    }

    std::unique_ptr<Order> createModified(ClientID clientID, OrderSide side,
                                          OrderType orderType, InstrumentID instrumentID,
                                          Qty qty, Price price, TimeInForce tif,
                                          Timestamp goodTill, uint32_t ref) {
        return std::make_unique<Order>(Order{
            ++idSqn_, clientID, side, orderType, instrumentID, qty, price, tif, goodTill,
            statusCodes::OrderStatus::MODIFIED, utils::getTimestampNs(), ref});
    }

private:
    OrderID idSqn_;
};