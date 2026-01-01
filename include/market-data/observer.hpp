#pragma once

#include "market-data/bookEvent.hpp"
#include "utils/spsc_queue.hpp"
#include "utils/types.hpp"

namespace market_data {
class Observer {
public:
    Observer(utils::spsc_queue_shm<OrderBookUpdate>* engineQueue,
             utils::spsc_queue<OrderBookUpdate>* mdQueue, Level2OrderBook& book,
             InstrumentID instrumentID)
        : engineQueue_(engineQueue), mdQueue_(mdQueue), book_(book),
          instrumentID_(instrumentID) {}

    ~Observer() = default;

    void drainQueue();

    template <OrderSide Side> auto getSnapshot() const {
        if constexpr (Side == OrderSide::BUY) {
            return book_.bids;
        } else {
            return book_.asks;
        }
    }

private:
    auto& getBook_(OrderSide side);
    bool priceBetterOrEqual_(Price incoming, Price resting, OrderSide side);

    void addAtPrice_(Price price, Qty amount, OrderSide side);
    void reduceAtPrice_(Price price, Qty amount, OrderSide side);

    utils::spsc_queue_shm<OrderBookUpdate>* engineQueue_;
    utils::spsc_queue<OrderBookUpdate>* mdQueue_;
    Level2OrderBook& book_;
    InstrumentID instrumentID_;
};

} // namespace market_data
