#include "core/matchingEngine.hpp"
#include "utils/timing.hpp"
#include "utils/types.hpp"
#include <memory>
#include <optional>

[[nodiscard]] MatchResult MatchingEngine::processOrder(std::unique_ptr<Order> order) {

    int sideIdx = (order->side == OrderSide::BUY ? 0 : 1);
    int typeIdx = (order->type == OrderType::LIMIT ? 0 : 1);

    return (this->*dispatchTable_[sideIdx][typeIdx])(std::move(order));
};

std::optional<Price> MatchingEngine::getBestAsk() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

std::optional<Price> MatchingEngine::getBestBid() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}

std::optional<Price> MatchingEngine::getSpread() const {
    if (asks_.empty() || bids_.empty()) return std::nullopt;
    return asks_.begin()->first - bids_.begin()->first;
}

void MatchingEngine::addToBook_(std::unique_ptr<Order> order) {
    Order* raw = order.get();

    if (order->side == OrderSide::BUY) {
        bids_[order->price].emplace_back(std::move(order));
    } else {
        asks_[order->price].emplace_back(std::move(order));
    }
    orderMap_[raw->orderID] = raw;
}

void MatchingEngine::reset() {
    bids_.clear();
    asks_.clear();
    orderMap_.clear();
}

[[nodiscard]] bool MatchingEngine::cancelOrder(const ClientID clientID,
                                               const OrderID orderID) {
    std::unordered_map<OrderID, Order*>::iterator it = orderMap_.find(orderID);
    if (it == orderMap_.end()) {
        return false;
    }

    if (it->second->clientID != clientID) {
        return false;
    }

    bool removed = (it->second->side == OrderSide::BUY)
                       ? (removeFromBook_(orderID, it->second->price, bids_))
                       : (removeFromBook_(orderID, it->second->price, asks_));

    return removed;
}

[[nodiscard]] ModifyResult MatchingEngine::modifyOrder(const ClientID clientID,
                                                       const OrderID orderID,
                                                       const Qty newQty,
                                                       const Price newPrice) {
    auto it = orderMap_.find(orderID);
    if (it == orderMap_.end()) {
        return {.serverClientID = clientID,
                .oldOrderID = orderID,
                .newOrderID = 0,
                .newQty = newQty,
                .newPrice = newPrice,
                .status = ModifyStatus::NOT_FOUND,
                .instrumentID = instrumentID_,
                .matchResult = std::nullopt};
    }

    auto& order = it->second;
    if (order->clientID != clientID) {
        return {.serverClientID = clientID,
                .oldOrderID = orderID,
                .newOrderID = 0,
                .newQty = newQty,
                .newPrice = newPrice,
                .status = ModifyStatus::INVALID,
                .instrumentID = instrumentID_,
                .matchResult = std::nullopt};
    }

    if (newQty <= 0 || newPrice <= 0) {
        return {.serverClientID = clientID,
                .oldOrderID = orderID,
                .newOrderID = 0,
                .newQty = newQty,
                .newPrice = newPrice,
                .status = ModifyStatus::INVALID,
                .instrumentID = instrumentID_,
                .matchResult = std::nullopt};
    }

    if (newPrice == order->price && newQty == order->qty) {
        return {.serverClientID = clientID,
                .oldOrderID = orderID,
                .newOrderID = orderID,
                .newQty = newQty,
                .newPrice = newPrice,
                .status = ModifyStatus::ACCEPTED,
                .instrumentID = instrumentID_,
                .matchResult = std::nullopt};
    }

    if (newPrice == order->price && newQty < order->qty) {
        order->qty = newQty;
        order->status = OrderStatus::MODIFIED;

        return {.serverClientID = clientID,
                .oldOrderID = orderID,
                .newOrderID = orderID,
                .newQty = newQty,
                .newPrice = newPrice,
                .status = ModifyStatus::ACCEPTED,
                .instrumentID = instrumentID_,
                .matchResult = std::nullopt};
    }

    OrderSide tmpSide = order->side;
    TimeInForce tmpTif = order->tif;
    Timestamp tmpGoodTill = order->goodTill;

    if (!cancelOrder(clientID, orderID)) {
        return {.serverClientID = clientID,
                .oldOrderID = orderID,
                .newOrderID = 0,
                .newQty = newQty,
                .newPrice = newPrice,
                .status = ModifyStatus::NOT_FOUND,
                .instrumentID = instrumentID_,
                .matchResult = std::nullopt};
    }

    // now, the order is cancelled, and the pointer [order] is invalidated

    std::unique_ptr<Order> newOrder = std::make_unique<Order>(
        getNextOrderID_(), clientID, newQty, newPrice, tmpGoodTill, TSCClock::now(),
        instrumentID_, tmpTif, tmpSide, OrderType::LIMIT, OrderStatus::MODIFIED);

    int sideIdx = (newOrder->side == OrderSide::BUY ? 0 : 1);
    int typeIdx = 0; // always limit

    OrderID tmpNewOrderID = newOrder->orderID;

    MatchResult matchResult =
        (this->*dispatchTable_[sideIdx][typeIdx])(std::move(newOrder));

    // now the newOrder has been moved into the book, and ownership has been handed over
    return {.serverClientID = clientID,
            .oldOrderID = orderID,
            .newOrderID = tmpNewOrderID,
            .newQty = newQty,
            .newPrice = newPrice,
            .status = ModifyStatus::ACCEPTED,
            .instrumentID = instrumentID_,
            .matchResult = matchResult};
}
