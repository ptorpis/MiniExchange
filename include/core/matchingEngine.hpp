#pragma once

#include <chrono>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/order.hpp"
#include "core/service.hpp"
#include "events/eventBus.hpp"
#include "events/events.hpp"
#include "utils/timing.hpp"
#include "utils/types.hpp"
#include "utils/utils.hpp"

using OrderQueue = std::deque<std::unique_ptr<Order>>;

class MatchingEngine {
public:
    MatchingEngine(std::shared_ptr<EventBus> evBus = nullptr,
                   InstrumentID instrumentID = 1)
        : evBus_(evBus), instrument_(instrumentID) {
        dispatchTable_[0][0] = &MatchingEngine::matchOrder_<BuySide, LimitOrderPolicy>;
        dispatchTable_[0][1] = &MatchingEngine::matchOrder_<BuySide, MarketOrderPolicy>;
        dispatchTable_[1][0] = &MatchingEngine::matchOrder_<SellSide, LimitOrderPolicy>;
        dispatchTable_[1][1] = &MatchingEngine::matchOrder_<SellSide, MarketOrderPolicy>;
        service_ = OrderService();
    }

    MatchResult processOrder(const OrderRequest& req);
    bool cancelOrder(const ClientID clientID, const OrderID orderID);
    ModifyResult modifyOrder(const ClientID clientID, const OrderID orderID,
                             const Qty newQty, const Price newPrice);
    void reset();

    std::optional<Price> getSpread() const;
    std::optional<Price> getBestAsk() const;
    std::optional<Price> getBestBid() const;

    std::optional<const Order*> getOrder(OrderID orderID) const;

    size_t getAskSize() const { return asks_.size(); }
    size_t getBidsSize() const { return bids_.size(); }

    size_t getAsksPriceLevelSize(Price price) { return asks_.at(price).size(); }
    size_t getBidsPriceLevelSize(Price price) { return bids_.at(price).size(); }

    OrderRequest createRequestFromMessage(Message<client::NewOrderPayload>& msg) {
        return service_.createRequestFromMessage(msg);
    }

    std::map<Price, Qty, std::greater<Price>> getBidsSnapshot() const {
        std::map<Price, Qty, std::greater<Price>> snapshot;
        for (const auto& [price, queue] : bids_) {
            Qty total = 0;
            for (const auto& order : queue) {
                total += order->qty;
            }
            snapshot[price] = total;
        }
        return snapshot;
    }

    std::map<Price, Qty, std::less<Price>> getAsksSnapshot() const {
        std::map<Price, Qty, std::less<Price>> snapshot;
        for (const auto& [price, queue] : asks_) {
            Qty total = 0;
            for (const auto& order : queue) {
                total += order->qty;
            }
            snapshot[price] = total;
        }
        return snapshot;
    }

private:
    std::map<Price, OrderQueue, std::less<Price>> asks_;
    std::map<Price, OrderQueue, std::greater<Price>> bids_;
    std::unordered_map<OrderID, Order*> orderMap_;

    using MatchFn = MatchResult (MatchingEngine::*)(std::unique_ptr<Order>);
    MatchFn dispatchTable_[2][2];

    template <typename SidePolicy, typename OrderTypePolicy>
    MatchResult matchOrder_(std::unique_ptr<Order> order);

    struct BuySide {

        static auto& book(MatchingEngine& eng) { return eng.asks_; }
        static TradeEvent makeTradeEvent(TradeID tradeID, Order* taker, Order* maker,
                                         Price p, Qty q, Timestamp ts,
                                         MatchingEngine& eng) {
            TradeEvent tradeEv{tradeID,
                               taker->orderID,
                               maker->orderID,
                               taker->clientID,
                               maker->clientID,
                               q,
                               p,
                               ts};
            eng.evBus_->publish<TradeEvent>({TSCClock::now(), tradeEv});
            return tradeEv;
        }
        static bool pricePasses(Price orderPrice, Price bestPrice) {
            return orderPrice >= bestPrice;
        }
    };

    struct SellSide {
        static auto& book(MatchingEngine& eng) { return eng.bids_; }
        static TradeEvent makeTradeEvent(TradeID tradeID, Order* taker, Order* maker,
                                         Price p, Qty q, Timestamp ts,
                                         MatchingEngine& eng) {
            TradeEvent tradeEv{tradeID,
                               maker->orderID,
                               taker->orderID,
                               maker->clientID,
                               taker->clientID,
                               q,
                               p,
                               ts};
            eng.evBus_->publish<TradeEvent>({TSCClock::now(), tradeEv});
            return tradeEv;
        }

        static bool pricePasses(Price orderPrice, Price bestPrice) {
            return orderPrice <= bestPrice;
        }
    };

    struct LimitOrderPolicy {
        static constexpr bool needsPriceCheck = true;
        static statusCodes::OrderStatus finalize(std::unique_ptr<Order> order,
                                                 Qty remaining, Qty original,
                                                 MatchingEngine& eng) {
            statusCodes::OrderStatus status = statusCodes::OrderStatus::NEW;
            if (!remaining) {
                order->status = statusCodes::OrderStatus::FILLED;
                status = statusCodes::OrderStatus::FILLED;
            } else {
                if (remaining < original) {
                    order->status = statusCodes::OrderStatus::PARTIALLY_FILLED;
                    status = statusCodes::OrderStatus::PARTIALLY_FILLED;
                }
                order->qty = remaining;
                eng.addToBook_(std::move(order));
            }

            return status;
        }
    };

    struct MarketOrderPolicy {
        static constexpr bool needsPriceCheck = false;
        static statusCodes::OrderStatus finalize(std::unique_ptr<Order> order,
                                                 Qty remaining, Qty original,
                                                 MatchingEngine&) {
            statusCodes::OrderStatus status = statusCodes::OrderStatus::NEW;
            if (!remaining) {
                order->status = statusCodes::OrderStatus::FILLED;
                status = statusCodes::OrderStatus::FILLED;
            } else if (remaining != original) {
                order->status = statusCodes::OrderStatus::PARTIALLY_FILLED;
                order->qty = remaining;
                status = statusCodes::OrderStatus::PARTIALLY_FILLED;
            } else {
                order->status = statusCodes::OrderStatus::CANCELLED;
                status = statusCodes::OrderStatus::CANCELLED;
            }

            return status;
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

    std::shared_ptr<EventBus> evBus_;
    InstrumentID instrument_;
};

template <typename SidePolicy, typename OrderTypePolicy>
MatchResult MatchingEngine::matchOrder_(std::unique_ptr<Order> order) {
    std::vector<TradeEvent> trades;
    Qty remainingQty = order->qty;
    const Qty originalQty = remainingQty;
    auto& book = SidePolicy::book(*this);

    while (remainingQty && !book.empty()) {
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

            // skip self trading
            if (restingOrder->clientID == order->clientID) {
                ++qIt;
                continue;
            }

            matched = true;

            Qty matchQty = std::min(remainingQty, restingOrder->qty);

            trades.emplace_back(SidePolicy::makeTradeEvent(
                getNextTradeID_(), order.get(), restingOrder, bestPrice, matchQty,
                currentTimestamp_(), *this));

            remainingQty -= matchQty;
            restingOrder->qty -= matchQty;

            if (!restingOrder->qty) {
                restingOrder->status = statusCodes::OrderStatus::FILLED;
                orderMap_.erase(restingOrder->orderID);
                qIt = queue.erase(qIt);
            } else {
                restingOrder->status = statusCodes::OrderStatus::PARTIALLY_FILLED;
                ++qIt;
            }
        }

        if (!matched) {
            break;
        }

        if (queue.empty()) {
            book.erase(bestPrice);
        }
    }

    // release ownership of the order, place it in the correct book / discard depending
    // on the policy
    OrderID orderID = order->orderID;
    Timestamp timestamp = order->timestamp;
    statusCodes::OrderStatus status =
        OrderTypePolicy::finalize(std::move(order), remainingQty, originalQty, *this);

    MatchResult result(orderID, timestamp, status, trades);

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

            evBus_->publish<RemoveFromBookEvent>(
                ServerEvent<RemoveFromBookEvent>{TSCClock::now(), {orderID}});

            return true;
        }
    }

    return false;
}
