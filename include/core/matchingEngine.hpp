#pragma once

#include <chrono>
#include <deque>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

#include "core/order.hpp"
#include "utils/types.hpp"

using OrderQueue = std::deque<Order*>;

struct TradeEvent {
    OrderID buyerOrderID;
    OrderID sellerOrderID;
    ClientID buyerID;
    ClientID sellerID;
    Qty qty;
    Price price;
    Timestamp timestamp;
};

class MatchingEngine {
public:
    MatchingEngine() {
        dispatchTable_[0][0] = &MatchingEngine::matchOrder_<BuySide, LimitOrderPolicy>;
        dispatchTable_[0][1] = &MatchingEngine::matchOrder_<BuySide, MarketOrderPolicy>;
        dispatchTable_[1][0] = &MatchingEngine::matchOrder_<SellSide, LimitOrderPolicy>;
        dispatchTable_[1][1] = &MatchingEngine::matchOrder_<SellSide, MarketOrderPolicy>;
    }

    std::vector<TradeEvent> processOrder(Order* order);
    bool cancelOrder(const OrderID, const ClientID clientID);
    void reset();

    std::optional<Price> getSpread() const;
    std::optional<Price> getBestAsk() const;
    std::optional<Price> getBestBid() const;

    std::optional<const Order*> getOrder(OrderID orderID) const;

    size_t getAskSize() const { return asks_.size(); }
    size_t getBidsSize() const { return bids_.size(); }

private:
    std::map<Price, OrderQueue, std::less<Price>> asks_;
    std::map<Price, OrderQueue, std::greater<Price>> bids_;
    std::unordered_map<OrderID, Order*> orderMap_;

    using MatchFn = std::vector<TradeEvent> (MatchingEngine::*)(Order*);
    MatchFn dispatchTable_[2][2];

    struct BuySide {

        static auto& book(MatchingEngine& eng) { return eng.asks_; }
        static TradeEvent makeTradeEvent(Order* taker, Order* maker, Price p, Qty q,
                                         Timestamp ts) {
            return TradeEvent{taker->clientID,
                              maker->clientID,
                              taker->orderID,
                              maker->orderID,
                              q,
                              p,
                              ts};
        }
        static bool pricePasses(Price orderPrice, Price bestPrice) {
            return orderPrice >= bestPrice;
        }
    };

    struct SellSide {
        static auto& book(MatchingEngine& eng) { return eng.bids_; }
        static TradeEvent makeTradeEvent(Order* taker, Order* maker, Price p, Qty q,
                                         Timestamp ts) {
            return TradeEvent{maker->clientID,
                              taker->clientID,
                              maker->orderID,
                              taker->orderID,
                              q,
                              p,
                              ts};
        }

        static bool pricePasses(Price orderPrice, Price bestPrice) {
            return orderPrice <= bestPrice;
        }
    };

    struct LimitOrderPolicy {
        static constexpr bool needsPriceCheck = true;
        static void finalize(Order* order, Qty remaining, Qty original,
                             MatchingEngine& eng) {
            if (!remaining) {
                order->status = OrderStatus::FILLED;
            } else {
                if (remaining < original) {
                    order->status = OrderStatus::PARTIALLY_FILLED;
                }
                order->qty = remaining;
                eng.addToBook_(order);
            }
        }
    };

    struct MarketOrderPolicy {
        static constexpr bool needsPriceCheck = false;
        static void finalize(Order* order, Qty remaining, Qty original, MatchingEngine&) {
            if (!remaining) {
                order->status = OrderStatus::FILLED;
            } else if (remaining != original) {
                order->status = OrderStatus::PARTIALLY_FILLED;
                order->qty = remaining;
            } else {
                order->status = OrderStatus::CANCELLED;
            }
        }
    };

    template <typename SidePolicy, typename OrderTypePolicy>
    std::vector<TradeEvent> matchOrder_(Order* order);

    void addToBook_(Order* order);

    static Timestamp currentTimestamp_() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }
};

template <typename SidePolicy, typename OrderTypePolicy>
std::vector<TradeEvent> MatchingEngine::matchOrder_(Order* order) {
    std::vector<TradeEvent> trades;
    Qty remainingQty = order->qty;
    const Qty originalQty = remainingQty;
    auto& book = SidePolicy::book(*this);

    auto it = book.begin();
    while (remainingQty && !book.empty()) {
        Price bestPrice = it->first;

        if constexpr (OrderTypePolicy::needsPriceCheck) {
            if (!SidePolicy::pricePasses(order->price, bestPrice)) {
                break;
            }
        }
        auto& queue = it->second;

        while (remainingQty && !queue.empty()) {
            auto restingOrder = queue.front();
            Qty matchQty = std::min(remainingQty, restingOrder->qty);

            trades.emplace_back(SidePolicy::makeTradeEvent(
                order, restingOrder, bestPrice, matchQty, currentTimestamp_()));

            remainingQty -= matchQty;
            restingOrder->qty -= matchQty;

            if (!restingOrder->qty) {
                restingOrder->status = OrderStatus::FILLED;
                queue.pop_front();
                orderMap_.erase(restingOrder->orderID);
            } else {
                restingOrder->status = OrderStatus::PARTIALLY_FILLED;
            }
        }

        if (queue.empty()) {
            book.erase(bestPrice);
        }
    }

    OrderTypePolicy::finalize(order, remainingQty, originalQty, *this);

    return trades;
}
