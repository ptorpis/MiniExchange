#include "core/matchingEngine.hpp"

MatchResult MatchingEngine::processOrder(const OrderRequest& req) {

    std::unique_ptr<Order> order = service_.orderFromRequest(req);

    int sideIdx = (order->side == OrderSide::BUY ? 0 : 1);
    int typeIdx = (order->type == OrderType::LIMIT ? 0 : 1);

    return (this->*dispatchTable_[sideIdx][typeIdx])(std::move(order));
};

void MatchingEngine::addToBook_(std::unique_ptr<Order> order) {
    Order* raw = order.get();

    if (order->side == OrderSide::BUY) {
        bids_[order->price].push_back(std::move(order));
    } else {
        asks_[order->price].push_back(std::move(order));
    }
    orderMap_[raw->orderID] = raw;

    evBus_->publish<AddedToBookEvent>(ServerEvent<AddedToBookEvent>{
        utils::getTimestampNs(),
        {raw->orderID, raw->clientID, raw->side, raw->qty, raw->price, raw->tif,
         raw->goodTill, raw->instrumentID}});
}

void MatchingEngine::reset() {
    bids_.clear();
    asks_.clear();
    orderMap_.clear();
}

bool MatchingEngine::cancelOrder(const ClientID clientID, const OrderID orderID) {
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

    if (removed) {
        orderMap_.erase(it);
        evBus_->publish<OrderCancelledEvent>(
            ServerEvent<OrderCancelledEvent>{utils::getTimestampNs(), orderID});
    }

    return removed;
}

ModifyResult MatchingEngine::modifyOrder(const ClientID clientID, const OrderID orderID,
                                         const Qty newQty, const Price newPrice) {
    auto it = orderMap_.find(orderID);
    if (it == orderMap_.end()) {
        return {ModifyEvent{clientID, orderID, 0, newQty, newPrice,
                            statusCodes::ModifyAckStatus::NOT_FOUND},
                std::nullopt};
    }

    auto& order = it->second;
    if (order->clientID != clientID) {
        return {ModifyEvent{clientID, orderID, 0, newQty, newPrice,
                            statusCodes::ModifyAckStatus::INVALID},
                std::nullopt};
    }

    if (newQty <= 0 || newPrice <= 0) {
        return {ModifyEvent{clientID, orderID, 0, newQty, newPrice,
                            statusCodes::ModifyAckStatus::INVALID},
                std::nullopt};
    }

    if (newPrice == order->price && newQty == order->qty) {
        ModifyEvent modEv{clientID, orderID,  orderID,
                          newQty,   newPrice, statusCodes::ModifyAckStatus::ACCEPTED};
        evBus_->publish<ModifyEvent>({utils::getTimestampNs(), modEv});

        return {modEv, std::nullopt};
    }

    if (newPrice == order->price && newQty < order->qty) {
        order->qty = newQty;
        order->status = statusCodes::OrderStatus::MODIFIED;

        ModifyEvent modEv{clientID, orderID,  orderID,
                          newQty,   newPrice, statusCodes::ModifyAckStatus::ACCEPTED};
        evBus_->publish<ModifyEvent>({utils::getTimestampNs(), modEv});

        return {modEv, std::nullopt};
    }

    OrderSide tmpSide = order->side;
    OrderType tmpType = order->type;
    InstrumentID tmpInstrID = order->instrumentID;
    TimeInForce tmpTif = order->tif;
    Timestamp tmpGoodTill = order->goodTill;

    if (!cancelOrder(clientID, orderID)) {
        return {ModifyEvent{clientID, orderID, 0, newQty, newPrice,
                            statusCodes::ModifyAckStatus::NOT_FOUND},
                std::nullopt};
    }

    // now, the order is cancelled, and the pointer [order] is invalidated

    std::unique_ptr<Order> newOrder = service_.createModified(
        clientID, tmpSide, tmpType, tmpInstrID, newQty, newPrice, tmpTif, tmpGoodTill);

    int sideIdx = (newOrder->side == OrderSide::BUY ? 0 : 1);
    int typeIdx = 0; // always limit

    OrderID tmpNewOrderID = newOrder->orderID;

    MatchResult matchResult =
        (this->*dispatchTable_[sideIdx][typeIdx])(std::move(newOrder));

    // now the newOrder has been moved into the book, and ownership has been handed over

    ModifyEvent modEv{clientID, orderID,  tmpNewOrderID,
                      newQty,   newPrice, statusCodes::ModifyAckStatus::ACCEPTED};
    evBus_->publish<ModifyEvent>({utils::getTimestampNs(), modEv});

    return {modEv, matchResult};
}

std::optional<const Order*> MatchingEngine::getOrder(OrderID orderID) const {
    auto it = orderMap_.find(orderID);
    if (it == orderMap_.end()) return std::nullopt;
    return it->second;
}

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