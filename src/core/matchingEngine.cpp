#include "core/matchingEngine.hpp"

std::vector<TradeEvent> MatchingEngine::processOrder(Order* order) {
    if (!order) {
        return {};
    }

    int sideIdx = (order->side == OrderSide::BUY ? 0 : 1);
    int typeIdx = (order->type == OrderType::LIMIT ? 0 : 1);

    return (this->*dispatchTable_[sideIdx][typeIdx])(order);
}

void MatchingEngine::addToBook_(Order* order) {
    if (!order) {
        return;
    }
    if (order->side == OrderSide::BUY) {
        bids_[order->price].push_back(order);
    } else {
        asks_[order->price].push_back(order);
    }

    orderMap_[order->orderID] = order;
}

void MatchingEngine::reset() {
    bids_.clear();
    asks_.clear();
    orderMap_.clear();
}