#pragma once

#include <chrono>
#include <deque>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/order.hpp"
#include "core/service.hpp"
#include "core/trade.hpp"
#include "utils/types.hpp"

using OrderQueue = std::deque<std::unique_ptr<Order>>;

class MatchingEngine {
public:
    MatchingEngine() {
        dispatchTable_[0][0] = &MatchingEngine::matchOrder_<BuySide, LimitOrderPolicy>;
        dispatchTable_[0][1] = &MatchingEngine::matchOrder_<BuySide, MarketOrderPolicy>;
        dispatchTable_[1][0] = &MatchingEngine::matchOrder_<SellSide, LimitOrderPolicy>;
        dispatchTable_[1][1] = &MatchingEngine::matchOrder_<SellSide, MarketOrderPolicy>;
        service_ = OrderService();
    }

    MatchResult processOrder(OrderRequest& req);
    bool cancelOrder(const ClientID clientID, const OrderID orderID);
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

    using MatchFn = std::vector<TradeEvent> (MatchingEngine::*)(std::unique_ptr<Order>);
    MatchFn dispatchTable_[2][2];

    template <typename SidePolicy, typename OrderTypePolicy>
    std::vector<TradeEvent> matchOrder_(std::unique_ptr<Order> order);

    struct BuySide {

        static auto& book(MatchingEngine& eng) { return eng.asks_; }
        static TradeEvent makeTradeEvent(TradeID tradeID, Order* taker, Order* maker,
                                         Price p, Qty q, Timestamp ts) {
            return TradeEvent{tradeID,
                              taker->orderID,
                              maker->orderID,
                              taker->clientID,
                              maker->clientID,
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
        static TradeEvent makeTradeEvent(TradeID tradeID, Order* taker, Order* maker,
                                         Price p, Qty q, Timestamp ts) {
            return TradeEvent{tradeID,
                              maker->orderID,
                              taker->orderID,
                              maker->clientID,
                              taker->clientID,
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
        static void finalize(std::unique_ptr<Order> order, Qty remaining, Qty original,
                             MatchingEngine& eng) {
            if (!remaining) {
                order->status = OrderStatus::FILLED;
            } else {
                if (remaining < original) {
                    order->status = OrderStatus::PARTIALLY_FILLED;
                }
                order->qty = remaining;
                eng.addToBook_(std::move(order));
            }
        }
    };

    struct MarketOrderPolicy {
        static constexpr bool needsPriceCheck = false;
        static void finalize(std::unique_ptr<Order> order, Qty remaining, Qty original,
                             MatchingEngine&) {
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

    void addToBook_(std::unique_ptr<Order> order);

    template <typename Book>
    bool removeFromBook_(const OrderID orderID, const Price price, Book& book);

    static Timestamp currentTimestamp_() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }

    TradeID tradeID{0};
    TradeID getNextTradeID_() { return ++tradeID; }

    OrderService service_;
};

template <typename SidePolicy, typename OrderTypePolicy>
std::vector<TradeEvent> MatchingEngine::matchOrder_(std::unique_ptr<Order> order) {
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
            Order* restingOrder = queue.front().get();
            Qty matchQty = std::min(remainingQty, restingOrder->qty);

            trades.emplace_back(
                SidePolicy::makeTradeEvent(getNextTradeID_(), order.get(), restingOrder,
                                           bestPrice, matchQty, currentTimestamp_()));

            remainingQty -= matchQty;
            restingOrder->qty -= matchQty;

            if (!restingOrder->qty) {
                restingOrder->status = OrderStatus::FILLED;
                orderMap_.erase(restingOrder->orderID);
                queue.pop_front();
            } else {
                restingOrder->status = OrderStatus::PARTIALLY_FILLED;
            }
        }

        if (queue.empty()) {
            book.erase(bestPrice);
        }
    }

    // release ownership of the order, place it in the correct book / discard depending
    // on the policy
    OrderTypePolicy::finalize(std::move(order), remainingQty, originalQty, *this);

    return trades;
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
