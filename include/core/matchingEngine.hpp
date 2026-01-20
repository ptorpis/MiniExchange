#pragma once

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <unordered_map>
#include <utility>

#include "core/order.hpp"
#include "market-data/bookEvent.hpp"
#include "utils/spsc_queue.hpp"
#include "utils/timing.hpp"
#include "utils/types.hpp"

using OrderQueue = std::deque<std::unique_ptr<Order>>;

class MatchingEngine {
public:
    MatchingEngine(utils::spsc_queue_shm<OrderBookUpdate>* queue = nullptr,
                   InstrumentID instrumentID = InstrumentID{1})
        : instrumentID_(instrumentID), queue_(queue) {
        dispatchTable_[0][0] = &MatchingEngine::matchOrder_<BuySide, LimitOrderPolicy>;
        dispatchTable_[0][1] = &MatchingEngine::matchOrder_<BuySide, MarketOrderPolicy>;
        dispatchTable_[1][0] = &MatchingEngine::matchOrder_<SellSide, LimitOrderPolicy>;
        dispatchTable_[1][1] = &MatchingEngine::matchOrder_<SellSide, MarketOrderPolicy>;
    }

    MatchResult processOrder(std::unique_ptr<Order> order);
    bool cancelOrder(const ClientID clientID, const OrderID orderID);
    ModifyResult modifyOrder(const ClientID clientID, const OrderID orderID,
                             const Qty newQty, const Price newPrice);

    void reset();

    std::optional<Price> getSpread() const;
    std::optional<Price> getBestAsk() const;
    std::optional<Price> getBestBid() const;

    std::size_t getAskSize() const { return asks_.size(); }
    std::size_t getBidsSize() const { return bids_.size(); }

    const Order* getOrder(OrderID orderID) const;

    [[nodiscard]] InstrumentID getInstrumentID() const noexcept { return instrumentID_; }
    OrderID getNextOrderID() { return ++orderID_; }

    static std::vector<std::pair<Price, Qty>> makeSnapshot(const auto& book) {
        std::vector<std::pair<Price, Qty>> snapshot;
        snapshot.reserve(book.size());

        for (const auto& [price, queue] : book) {
            auto qtyView = std::views::transform([](const auto& order) {
                return order->qty;
            });

            Qty total = std::ranges::fold_left(queue | qtyView, Qty{0}, std::plus{});

            if (total > 0) {
                snapshot.emplace_back(price, total);
            }
        }

        std::ranges::reverse(snapshot);
        return snapshot;
    }

    template <OrderSide Side> std::vector<std::pair<Price, Qty>> getSnapshot() const {
        if constexpr (Side == OrderSide::BUY) {
            return makeSnapshot(bids_);
        } else {
            return makeSnapshot(asks_);
        }
    }

    constexpr bool isValidOrder(const Order& order) {
        std::uint8_t mask = 0;

        constexpr std::uint8_t PRICE_BIT = 1u << 0;
        constexpr std::uint8_t QTY_BIT = 1u << 1;
        constexpr std::uint8_t SIDE_BIT = 1u << 2;
        constexpr std::uint8_t TYPE_BIT = 1u << 3;
        constexpr std::uint8_t INSTRUMENT_BIT = 1u << 4;
        constexpr std::uint8_t MARKET_PRICE_BIT = 1u << 5;

        mask |= (order.type == OrderType::LIMIT && order.price.value() == 0)
                    ? PRICE_BIT
                    : std::uint8_t{0};

        mask |= (order.qty.value() == 0) ? QTY_BIT : std::uint8_t{0};
        mask |= (+order.side > 1) ? SIDE_BIT : std::uint8_t{0};
        mask |= (+order.type > 1) ? TYPE_BIT : std::uint8_t{0};

        mask |= (order.instrumentID.value() != instrumentID_) ? INSTRUMENT_BIT
                                                              : std::uint8_t{0};

        mask |= ((order.type == OrderType::MARKET && order.price.value() != 0)
                     ? MARKET_PRICE_BIT
                     : std::uint8_t{0});

        return mask == 0;
    }

private:
    InstrumentID instrumentID_;
    std::map<Price, OrderQueue, std::less<Price>> asks_;
    std::map<Price, OrderQueue, std::greater<Price>> bids_;
    std::unordered_map<OrderID, Order*> orderMap_;

    utils::spsc_queue_shm<OrderBookUpdate>* queue_;

    using MatchFunction = MatchResult (MatchingEngine::*)(std::unique_ptr<Order>);
    MatchFunction dispatchTable_[2][2];

    template <typename SidePolicy, typename OrderTypePolicy>
    MatchResult matchOrder_(std::unique_ptr<Order> order);

    void emitObserverEvent_(Price price, Qty amount, OrderSide side,
                            BookUpdateEventType type);

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
    OrderID orderID_{0};

    TradeID getNextTradeID_() { return ++tradeID_; }
};

template <typename SidePolicy, typename OrderTypePolicy>
MatchResult MatchingEngine::matchOrder_(std::unique_ptr<Order> order) {
    std::vector<TradeEvent> tradeVec{};

    Qty remainingQty = order->qty;
    const Qty originalQty = remainingQty;

    auto& book = SidePolicy::book(*this);

    Price bestPrice{};
    if (book.empty()) {
        bestPrice = order->price;
    }

    while (remainingQty.value() && !book.empty()) {
        auto it = book.begin();

        bestPrice = it->first;

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
            restingOrder->qty -= matchQty;
            remainingQty -= matchQty;

            OrderSide eventSide =
                SidePolicy::isBuyer() ? OrderSide::SELL : OrderSide::BUY;
            emitObserverEvent_(bestPrice, matchQty, eventSide,
                               BookUpdateEventType::REDUCE);

            OrderID sellerOrderID{}, buyerOrderID{};
            ClientID sellerID{}, buyerID{};
            ClientOrderID buyerClientOrderID{}, sellerClientOrderID{};

            if (SidePolicy::isBuyer()) {
                buyerID = order->clientID;
                sellerID = restingOrder->clientID;
                buyerOrderID = order->orderID;
                sellerOrderID = restingOrder->orderID;
                buyerClientOrderID = order->clientOrderID;
                sellerClientOrderID = restingOrder->clientOrderID;
            } else {
                sellerID = order->clientID;
                buyerID = restingOrder->clientID;
                sellerOrderID = order->orderID;
                buyerOrderID = restingOrder->orderID;
                sellerClientOrderID = order->clientOrderID;
                buyerClientOrderID = restingOrder->clientOrderID;
            }

            tradeVec.emplace_back(TradeEvent{.tradeID = getNextTradeID_(),
                                             .buyerOrderID = buyerOrderID,
                                             .sellerOrderID = sellerOrderID,
                                             .buyerID = buyerID,
                                             .sellerID = sellerID,
                                             .buyerClientOrderID = buyerClientOrderID,
                                             .sellerClientOrderID = sellerClientOrderID,
                                             .qty = matchQty,
                                             .price = bestPrice,
                                             .timestamp = TSCClock::now(),
                                             .instrumentID = instrumentID_});

            if (restingOrder->qty == 0) {
                restingOrder->status = OrderStatus::FILLED;
                orderMap_.erase(restingOrder->orderID);
                qIt = queue.erase(qIt);
            } else {
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

    OrderID orderID = order->orderID;
    Timestamp timestamp = order->timestamp;
    OrderStatus status =
        OrderTypePolicy::finalize(std::move(order), remainingQty, originalQty, *this);

    MatchResult result{
        .orderID = orderID,
        .timestamp = timestamp,
        .remainingQty = remainingQty,
        .acceptedPrice = bestPrice,
        .status = status,
        .instrumentID = instrumentID_,
        .tradeVec = tradeVec,
    };

    return result;
}

template <typename Book>
bool MatchingEngine::removeFromBook_(OrderID orderID, Price price, Book& book) {
    auto it = book.find(price);
    if (it == book.end()) {
        return false;
    }

    OrderQueue& queue = it->second;

    for (auto qIt = queue.begin(); qIt != queue.end(); ++qIt) {
        Order* order = qIt->get();

        if (order->orderID != orderID) {
            continue;
        }

#ifndef NDEBUG
        // the removal from the registry was not here in v2, it was in the caller. I moved
        // this here, because it keeps the invariant clearer, the orders that are removed
        // from the book are also being removed from the registry
        auto mapIt = orderMap_.find(orderID);
        assert(mapIt != orderMap_.end());
        assert(mapIt->second == order);
#endif
        emitObserverEvent_(order->price, order->qty, order->side,
                           BookUpdateEventType::REDUCE);

        orderMap_.erase(orderID); // BEFORE erasing from the queue -- UB otherwise
        queue.erase(qIt);

        if (queue.empty()) {
            book.erase(it);
        }

        return true;
    }

    return false;
}
