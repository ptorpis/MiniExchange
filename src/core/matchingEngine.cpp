#include "core/matchingEngine.hpp"

MatchResult MatchingEngine::processOrder(OrderRequest& req) {

    std::unique_ptr<Order> order = service_.orderFromRequest(req);

    int sideIdx = (order->side == OrderSide::BUY ? 0 : 1);
    int typeIdx = (order->type == OrderType::LIMIT ? 0 : 1);

    return {order->orderID, order->timestamp,
            (this->*dispatchTable_[sideIdx][typeIdx])(std::move(order))};
}

void MatchingEngine::addToBook_(std::unique_ptr<Order> order) {
    Order* raw = order.get();

    if (order->side == OrderSide::BUY) {
        bids_[order->price].push_back(std::move(order));
    } else {
        asks_[order->price].push_back(std::move(order));
    }
    orderMap_[raw->orderID] = raw;
}

void MatchingEngine::reset() {
    bids_.clear();
    asks_.clear();
    orderMap_.clear();
}

bool MatchingEngine::cancelOrder(const ClientID clientID, const OrderID orderID) {
    auto it = orderMap_.find(orderID);
    if (it == orderMap_.end()) return false;

    if (it->second->clientID != clientID) return false;

    bool removed = (it->second->side == OrderSide::BUY)
                       ? (removeFromBook_(orderID, it->second->price, bids_))
                       : (removeFromBook_(orderID, it->second->price, asks_));

    if (removed) {
        orderMap_.erase(it);
    }

    return removed;
}
