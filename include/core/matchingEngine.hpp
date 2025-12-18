#pragma once

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>

#include "core/order.hpp"
#include "utils/timing.hpp"
#include "utils/types.hpp"

using OrderQueue = std::deque<std::unique_ptr<Order>>;

class MatchingEngine {
public:
    MatchingEngine(InstrumentID instrumentID = 1) : instrumentID_(instrumentID) {}

    MatchResult processOrder(std::unique_ptr<Order> order);
    bool cancelOrder(const ClientID clientID, const OrderID orderID);
    ModifyResult modifyOrder(const ClientID clientID, const OrderID orderID,
                             const Qty newQty, const Price newPrice);

    void reset();

    std::optional<Price> getSpread() const;
    std::optional<Price> getBestAsk() const;
    std::optional<Price> getBestBid() const;

private:
    InstrumentID instrumentID_;
    std::map<Price, OrderQueue, std::less<Price>> asks_;
    std::map<Price, OrderQueue, std::greater<Price>> bids_;
    std::unordered_map<OrderID, Order*> orderMap_;

    using MatchFunction = MatchResult (MatchingEngine::*)(std::unique_ptr<Order>);
    MatchFunction dispatchTable_[2][2];

    template <typename SidePolicy, typename OrderTypePolicy>
    MatchResult matchOrder_(std::unique_ptr<Order> order);

    struct BuySide {
        constexpr static auto& book(MatchingEngine& eng) { return eng.asks_; }
        static bool pricePasses(Price orderPrice, Price bestPrice) {
            return orderPrice >= bestPrice;
        }
        constexpr static bool isBuyer() { return true; }
    };

    struct SellSide {
        constexpr static auto& book(MatchingEngine& eng) { return eng.bids_; }
        static bool pricePasses(Price orderPrice, Price bestPrice) {
            return orderPrice <= bestPrice;
        }
        constexpr static bool isBuyer() { return false; }
    };

    struct LimitOrderPolicy {
        constexpr static bool needsPriceCheck = true;
        static OrderStatus finalize(std::unique_ptr<Order> order, Qty remaining,
                                    Qty original, [[maybe_unused]] MatchingEngine& eng) {
            OrderStatus status = OrderStatus::NEW;
            if (!remaining) {
                order->status = OrderStatus::FILLED;
                status = OrderStatus::FILLED;
            } else {
                if (remaining < original) {
                    order->status = OrderStatus::PARTIALLY_FILLED;
                    status = OrderStatus::PARTIALLY_FILLED;
                }
                order->qty = remaining;
                eng.addToBook_(std::move(order));
            }

            return status;
        }
    };

    struct MarketOrderPolicy {
        constexpr static bool needsPriceCheck = false;
        static OrderStatus finalize(std::unique_ptr<Order> order, Qty remaining,
                                    Qty original, MatchingEngine&) {
            OrderStatus status = OrderStatus::NEW;
            if (!remaining) {
                order->status = OrderStatus::FILLED;
                status = OrderStatus::FILLED;
            } else if (remaining != original) {
                order->status = OrderStatus::PARTIALLY_FILLED;
                order->qty = remaining;
                status = OrderStatus::PARTIALLY_FILLED;
            } else {
                order->status = OrderStatus::CANCELLED;
                status = OrderStatus::CANCELLED;
            }

            return status;
        }
    };

    void addToBook_(std::unique_ptr<Order> order);

    template <typename Book>
    bool removeFromBook_(const OrderID orderID, const Price price, Book& book);

    TradeID tradeID_{0};
    OrderID orderIDSqn_{0};

    TradeID getNextTradeID_() { return ++tradeID_; }
    OrderID getNextOrderID_() { return ++orderIDSqn_; }
};

template <typename SidePolicy, typename OrderTypePolicy>
MatchResult MatchingEngine::matchOrder_(std::unique_ptr<Order> order) {
    std::vector<TradeEvent> tradeVec{};

    Qty remainingQty = order->qty;
    const Qty originalQty = remainingQty;

    auto& book = SidePolicy::book(*this);

    while (remainingQty && book.empty()) {
        auto it = book.begin();
        Price bestPrice = it->first;

        if constexpr (OrderTypePolicy::needsPriceCheck) {
            if (!SidePolicy::pricePasses(order->price, bestPrice)) {
                break;
            }
        }

        auto& queue = it->second;

        bool matched = false;

        for (auto qIt = queue.begin(); qIt != queue.end() && remainingQty > 0;) {
            Order* restingOrder = qIt->get();
            if (restingOrder->clientID == order->clientID) {
                ++qIt;
                continue;
            }
            matched = true;

            Qty matchQty = std::min(remainingQty, restingOrder->qty);

            OrderID sellerOrderID{};
            OrderID buyerOrderID{};
            ClientID sellerID{};
            ClientID buyerID{};

            if (SidePolicy::isBuyer()) {
                buyerID = order->clientID;
                sellerID = restingOrder->clientID;
                buyerOrderID = order->orderID;
                sellerOrderID = restingOrder->orderID;
            } else {
                sellerID = order->clientID;
                buyerID = restingOrder->clientID;
                sellerOrderID = order->orderID;
                buyerOrderID = restingOrder->orderID;
            }

            TradeEvent tradeEv{.tradeID = getNextTradeID_(),
                               .buyerOrderID = buyerOrderID,
                               .sellerOrderID = sellerOrderID,
                               .buyerID = buyerID,
                               .sellerID = sellerID,
                               .qty = matchQty,
                               .price = bestPrice,
                               .timestamp = TSCClock::now(),
                               .instrumentID = instrumentID_};

            tradeVec.push_back(std::move(tradeEv));
        }

        if (!matched) {
            break;
        }

        if (queue.empty()) {
            book.erase(bestPrice);
        }
    }

    OrderID orderID = order->orderID;
    Timestamp timestamp = order->timestamp;
    OrderStatus status =
        OrderTypePolicy::finalize(std::move(order), remainingQty, originalQty, *this);

    MatchResult result(orderID, timestamp, remainingQty, status, tradeVec);

    return result;
}

template <typename Book> bool
MatchingEngine::removeFromBook_(const OrderID orderID, const Price price, Book& book) {
    auto it = book.find(price);
    if (it == book.end()) return false;

    OrderQueue& queue = it->second;

    for (auto qIt = queue.begin(); qIt != queue.end(); ++qIt) {
        if ((*qIt)->orderID == orderID) {
            queue.erase(qIt);

            if (queue.empty()) {
                book.erase(it);
            }

            return true;
        }
    }

    return false;
}
