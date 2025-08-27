#include "core/matchingEngine.hpp"

std::vector<TradeEvent> MatchingEngine::processOrder(Order* order) {
    if (!order) {
        return {};
    }

    int sideIdx = (order->side == OrderSide::BUY ? 0 : 1);
    int typeIdx = (order->type == OrderType::LIMIT ? 0 : 1);

    return std::move((this->*dispatchTable_[sideIdx][typeIdx])(order));
}

std::vector<TradeEvent> MatchingEngine::matchLimitBuy_(Order* order) {
    std::vector<TradeEvent> trades;

    Qty remainingQty = order->qty;
    const Qty originalQty = remainingQty;

    auto it = bids_.begin();
    while (remainingQty > 0 && !bids_.empty()) {
        Price bestPrice = it->first;

        if (order->price < bestPrice) {
            break;
        }

        auto& queue = it->second;

        while (!queue.empty() && remainingQty > 0) {
            auto restingOrder = queue.front();
            Qty matchQty = std::min(remainingQty, restingOrder->qty);

            ClientID buyerID = order->clientID;
            ClientID sellerID = restingOrder->clientID;
            OrderID buyerOrderID = order->orderID;
            OrderID sellerOrderID = restingOrder->orderID;

            Timestamp ts = currentTimestamp_();

            trades.emplace_back(TradeEvent{order->clientID, restingOrder->clientID,
                                           order->orderID, restingOrder->orderID,
                                           matchQty, bestPrice, ts});

            remainingQty -= matchQty;
            restingOrder->qty -= matchQty;

            if (restingOrder->qty == 0) {
                restingOrder->status = OrderStatus::FILLED;
                queue.pop_front();
                orderMap_.erase(restingOrder->orderID);
            } else {
                restingOrder->status = OrderStatus::PARTIALLY_FILLED;
            }
        }

        if (queue.empty()) {
            bids_.erase(bestPrice);
        }
    }

    if (remainingQty == 0) {
        order->status = OrderStatus::FILLED;
    } else {
        if (remainingQty < originalQty) {
            order->status = OrderStatus::PARTIALLY_FILLED;
        }

        order->qty = remainingQty;
        addToBids_(order);
    }

    return std::move(trades);
}

std::vector<TradeEvent> MatchingEngine::matchLimitSell_(Order* order) {
    std::vector<TradeEvent> trades;

    Qty remainingQty = order->qty;
    const Qty originalQty = remainingQty;

    auto it = asks_.begin();

    while (remainingQty > 0 && !asks_.empty()) {
        Price bestPrice = it->first;

        if (order->price > bestPrice) {
            break;
        }

        auto& queue = it->second;

        while (!queue.empty() && remainingQty > 0) {
            auto restingOrder = queue.front();
            Qty matchQty = std::min(remainingQty, restingOrder->qty);

            ClientID sellerID = order->clientID;
            ClientID buyerID = restingOrder->clientID;
            OrderID sellerOrderID = order->orderID;
            OrderID buyerOrderID = restingOrder->orderID;

            Timestamp ts = currentTimestamp_();

            trades.emplace_back(TradeEvent{order->clientID, restingOrder->clientID,
                                           order->orderID, restingOrder->orderID,
                                           matchQty, bestPrice, ts});

            remainingQty -= matchQty;
            restingOrder->qty -= matchQty;

            if (restingOrder->qty == 0) {
                restingOrder->status = OrderStatus::FILLED;
                queue.pop_front();
                orderMap_.erase(restingOrder->orderID);
            } else {
                restingOrder->status = OrderStatus::PARTIALLY_FILLED;
            }
        }

        if (queue.empty()) {
            asks_.erase(bestPrice);
        }
    }

    if (remainingQty == 0) {
        order->status = OrderStatus::FILLED;
    } else {
        if (remainingQty < originalQty) {
            order->status = OrderStatus::PARTIALLY_FILLED;
        }
        order->qty = remainingQty;
        addToAsks_(order);
    }

    return std::move(trades);
}

std::vector<TradeEvent> MatchingEngine::matchMarketBuy_(Order* order) {
    std::vector<TradeEvent> trades;

    Qty remainingQty = order->qty;
    const Qty originalQty = remainingQty;

    auto it = bids_.begin();
    while (remainingQty > 0 && !bids_.empty()) {
        Price bestPrice = it->first;

        auto& queue = it->second;

        while (!queue.empty() && remainingQty > 0) {
            auto restingOrder = queue.front();
            Qty matchQty = std::min(remainingQty, restingOrder->qty);

            ClientID buyerID = order->clientID;
            ClientID sellerID = restingOrder->clientID;
            OrderID buyerOrderID = order->orderID;
            OrderID sellerOrderID = restingOrder->orderID;

            Timestamp ts = currentTimestamp_();

            trades.emplace_back(TradeEvent{order->clientID, restingOrder->clientID,
                                           order->orderID, restingOrder->orderID,
                                           matchQty, bestPrice, ts});

            remainingQty -= matchQty;
            restingOrder->qty -= matchQty;

            if (restingOrder->qty == 0) {
                restingOrder->status = OrderStatus::FILLED;
                queue.pop_front();
                orderMap_.erase(restingOrder->orderID);
            } else {
                restingOrder->status = OrderStatus::PARTIALLY_FILLED;
            }
        }

        if (queue.empty()) {
            bids_.erase(bestPrice);
        }
    }

    if (remainingQty == 0) {
        order->status = OrderStatus::FILLED;
    } else if (remainingQty != originalQty) {
        order->status = OrderStatus::PARTIALLY_FILLED;
        order->qty = remainingQty;
    } else {
        order->status = OrderStatus::CANCELLED;
    }

    return std::move(trades);
}

std::vector<TradeEvent> MatchingEngine::matchMarketSell_(Order* order) {
    std::vector<TradeEvent> trades;

    Qty remainingQty = order->qty;
    const Qty originalQty = remainingQty;

    auto it = asks_.begin();
    while (remainingQty > 0 && !asks_.empty()) {
        Price bestPrice = it->first;

        auto& queue = it->second;

        while (!queue.empty() && remainingQty > 0) {
            auto restingOrder = queue.front();
            Qty matchQty = std::min(remainingQty, restingOrder->qty);

            ClientID buyerID = order->clientID;
            ClientID sellerID = restingOrder->clientID;
            OrderID buyerOrderID = order->orderID;
            OrderID sellerOrderID = restingOrder->orderID;

            Timestamp ts = currentTimestamp_();

            trades.emplace_back(TradeEvent{order->clientID, restingOrder->clientID,
                                           order->orderID, restingOrder->orderID,
                                           matchQty, bestPrice, ts});

            remainingQty -= matchQty;
            restingOrder->qty -= matchQty;

            if (restingOrder->qty == 0) {
                restingOrder->status = OrderStatus::FILLED;
                queue.pop_front();
                orderMap_.erase(restingOrder->orderID);
            } else {
                restingOrder->status = OrderStatus::PARTIALLY_FILLED;
            }
        }

        if (queue.empty()) {
            asks_.erase(bestPrice);
        }
    }

    if (remainingQty == 0) {
        order->status = OrderStatus::FILLED;
    } else if (remainingQty != originalQty) {
        order->status = OrderStatus::PARTIALLY_FILLED;
        order->qty = remainingQty;
    } else {
        order->status = OrderStatus::CANCELLED;
    }

    return std::move(trades);
}

void MatchingEngine::addToAsks_(Order* order) {
    if (!order) {
        return;
    }
    asks_[order->price].push_back(order);
    orderMap_[order->orderID] = order;
}

void MatchingEngine::addToBids_(Order* order) {
    if (!order) {
        return;
    }
    bids_[order->price].push_back(order);
    orderMap_[order->orderID] = order;
}

void MatchingEngine::reset() {
    bids_.clear();
    asks_.clear();
    orderMap_.clear();
}