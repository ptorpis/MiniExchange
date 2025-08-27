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
        dispatchTable_[0][0] = &MatchingEngine::matchLimitBuy_;
        dispatchTable_[0][1] = &MatchingEngine::matchMarketBuy_;
        dispatchTable_[1][0] = &MatchingEngine::matchLimitSell_;
        dispatchTable_[1][1] = &MatchingEngine::matchMarketSell_;
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

    std::vector<TradeEvent> matchLimitBuy_(Order* order);
    std::vector<TradeEvent> matchLimitSell_(Order* order);
    std::vector<TradeEvent> matchMarketBuy_(Order* order);
    std::vector<TradeEvent> matchMarketSell_(Order* order);

    void addToAsks_(Order* order);
    void addToBids_(Order* order);

    static Timestamp currentTimestamp_() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }
};
