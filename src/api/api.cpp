#include "api/api.hpp"
#include "protocol/clientMessages.hpp"
#include "utils/timing.hpp"
#include "utils/types.hpp"
#include <memory>

MatchResult MiniExchangeAPI::processNewOrder(const client::NewOrderPayload& payload) {

    // TODO: validate the order parameters
    std::unique_ptr<Order> order =
        std::make_unique<Order>(Order{.orderID = OrderID{engine_.getNextOrderID()},
                                      .clientID = ClientID{payload.serverClientID},
                                      .qty = Qty{payload.qty},
                                      .price = Price{payload.price},
                                      .goodTill = payload.goodTillDate,
                                      .timestamp = TSCClock::now(),
                                      .instrumentID = InstrumentID{payload.instrumentID},
                                      .tif = TimeInForce{payload.timeInForce},
                                      .side = OrderSide{payload.orderSide},
                                      .type = OrderType{payload.orderType},
                                      .status = OrderStatus::NEW});

    return engine_.processOrder(std::move(order));
}

bool MiniExchangeAPI::cancelOrder(const client::CancelOrderPayload& payload) {
    // TODO: validate the parameters
    return engine_.cancelOrder(ClientID{payload.serverClientID},
                               OrderID{payload.serverOrderID});
}

ModifyResult MiniExchangeAPI::modifyOrder(const client::ModifyOrderPayload& payload) {
    // TODO: validate the parameters
    return engine_.modifyOrder(ClientID{payload.serverClientID},
                               OrderID{payload.serverOrderID}, Qty{payload.newQty},
                               Price{payload.newPrice});
}
