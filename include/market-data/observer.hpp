#pragma once

#include "market-data/bookEvent.hpp"
#include "utils/spsc_queue.hpp"
#include "utils/types.hpp"

namespace market_data {
class Observer {
public:
    Observer(utils::spsc_queue_shm<L2OrderBookUpdate>* engineQueue,
             utils::spsc_queue<L2OrderBookUpdate>* mdQueue, Level2OrderBook& l2,
             Level3OrderBook& l3, InstrumentID instrumentID)
        : engineQueue_(engineQueue), mdQueue_(mdQueue), l2book_(l2), l3book_(l3),
          instrumentID_(instrumentID) {}

    ~Observer() = default;

    void drainQueue();

    template <OrderSide Side> auto getSnapshot() const {
        if constexpr (Side == OrderSide::BUY) {
            return l2book_.bids;
        } else {
            return l2book_.asks;
        }
    }

private:
    inline auto& getBook_(OrderSide side);

    bool priceBetterOrEqual_(Price incoming, Price resting, OrderSide side);

    void addAtPrice_(Price price, Qty amount, OrderSide side);
    void reduceAtPrice_(Price price, Qty amount, OrderSide side);

    utils::spsc_queue_shm<L2OrderBookUpdate>* engineQueue_;
    utils::spsc_queue<L2OrderBookUpdate>* mdQueue_;
    Level2OrderBook& l2book_;
    Level3OrderBook& l3book_;
    InstrumentID instrumentID_;
};

} // namespace market_data
