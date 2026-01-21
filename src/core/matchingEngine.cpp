#include "core/matchingEngine.hpp"
#include "market-data/bookEvent.hpp"
#include "utils/timing.hpp"
#include "utils/types.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>

[[nodiscard]] MatchResult MatchingEngine::processOrder(std::unique_ptr<Order> order) {
    std::uint64_t currentTime = TSCClock::now();

    if (!isValidOrder(*order)) {
        return MatchResult{.orderID = OrderID{0},
                           .timestamp = currentTime,
                           .remainingQty = order->qty,
                           .acceptedPrice = Price{0},
                           .status = OrderStatus::REJECTED,
                           .instrumentID = instrumentID_,
                           .tradeVec{}};
    }

    int sideIdx = (order->side == OrderSide::BUY ? 0 : 1);
    int typeIdx = (order->type == OrderType::LIMIT ? 0 : 1);

    return (this->*dispatchTable_[sideIdx][typeIdx])(std::move(order));
};

std::optional<Price> MatchingEngine::getBestAsk() const {
    if (book.asks.empty()) return std::nullopt;
    return book.asks.begin()->first;
}

std::optional<Price> MatchingEngine::getBestBid() const {
    if (book.bids.empty()) return std::nullopt;
    return book.bids.begin()->first;
}

std::optional<Price> MatchingEngine::getSpread() const {
    if (book.asks.empty() || book.bids.empty()) return std::nullopt;
    return book.asks.begin()->first - book.bids.begin()->first;
}

void MatchingEngine::addToBook_(std::unique_ptr<Order> order) {
    Order* raw = order.get();

    emitObserverEvent_(order->price, order->qty, order->side, BookUpdateEventType::ADD);

    // LEVEL 3 ADD ORDER EVENT HERE

    if (order->side == OrderSide::BUY) {
        book.bids[order->price].emplace_back(std::move(order));
    } else {
        book.asks[order->price].emplace_back(std::move(order));
    }
    book.orderMap[raw->orderID] = raw;
}

void MatchingEngine::reset() {
    book.bids.clear();
    book.asks.clear();
    book.orderMap.clear();
}

[[nodiscard]] bool MatchingEngine::cancelOrder(const ClientID clientID,
                                               const OrderID orderID) {
    auto it = book.orderMap.find(orderID);
    if (it == book.orderMap.end()) {
        return false;
    }

    if (it->second->clientID != clientID) {
        return false;
    }

    bool removed = (it->second->side == OrderSide::BUY)
                       ? (removeFromBook_(orderID, it->second->price, book.bids))
                       : (removeFromBook_(orderID, it->second->price, book.asks));

    return removed;
}

[[nodiscard]] ModifyResult MatchingEngine::modifyOrder(const ClientID clientID,
                                                       const OrderID orderID,
                                                       const Qty newQty,
                                                       const Price newPrice) {
    auto it = book.orderMap.find(orderID);
    if (it == book.orderMap.end()) {
        return {.serverClientID = clientID,
                .oldOrderID = orderID,
                .newOrderID = OrderID{0},
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
                .newOrderID = OrderID{0},
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

        Qty delta = order->qty - newQty;
        emitObserverEvent_(newPrice, delta, order->side, BookUpdateEventType::REDUCE);

        // REDUDE LEVEL 3 event

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
    ClientOrderID tmpClientOrderID = order->clientOrderID;

    if (!cancelOrder(clientID, orderID)) {
        return {.serverClientID = clientID,
                .oldOrderID = orderID,
                .newOrderID = OrderID{0},
                .newQty = newQty,
                .newPrice = newPrice,
                .status = ModifyStatus::NOT_FOUND,
                .instrumentID = instrumentID_,
                .matchResult = std::nullopt};
    }

    // now, the order is cancelled, and the pointer [order] is invalidated

    std::unique_ptr<Order> newOrder =
        std::make_unique<Order>(getNextOrderID(), clientID, tmpClientOrderID, newQty,
                                newPrice, tmpGoodTill, TSCClock::now(), instrumentID_,
                                tmpTif, tmpSide, OrderType::LIMIT, OrderStatus::MODIFIED);

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

const Order* MatchingEngine::getOrder(OrderID orderID) const {
    if (auto it = book.orderMap.find(orderID); it != book.orderMap.end()) {
        return it->second;
    } else {
        return nullptr;
    }
}
