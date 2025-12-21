#include "api/api.hpp"
#include "protocol/clientMessages.hpp"
#include "utils/timing.hpp"
#include "utils/types.hpp"
#include <memory>

MatchResult MiniExchangeAPI::processNewOrder(const client::NewOrderPayload& payload) {

    // TODO: validate the order parameters

    std::unique_ptr<Order> order =
        std::make_unique<Order>(Order{.orderID = OrderID{getNextOrderID_()},
                                      .clientID = ClientID{payload.clientID},
                                      .qty = Qty{payload.qty},
                                      .price = Price{payload.price},
                                      .goodTill = payload.goodTillDate,
                                      .timestamp = TSCClock::now(),
                                      .instrumentID = payload.instrumentID,
                                      .tif = TimeInForce{payload.timeInForce},
                                      .side = OrderSide{payload.orderSide},
                                      .type = OrderType{payload.orderType},
                                      .status = OrderStatus::NEW});

    return engine_.processOrder(std::move(order));
}

bool MiniExchangeAPI::cancelOrder(const client::CancelOrderPayload& payload) {
    // TODO: validate the parameters
    return engine_.cancelOrder(payload.serverClientID, payload.serverOrderID);
}

ModifyResult MiniExchangeAPI::modifyOrder(const client::ModifyOrderPayload& payload) {
    // TODO: validate the parameters
    return engine_.modifyOrder(payload.serverClientID, payload.serverOrderID,
                               Qty{payload.newQty}, Price{payload.newPrice});
}
