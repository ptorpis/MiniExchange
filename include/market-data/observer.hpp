#pragma once

#include "market-data/bookEvent.hpp"
#include "utils/spsc_queue.hpp"
#include "utils/types.hpp"
#include <utility>

using Level = std::pair<Price, Qty>;
using Book = std::vector<Level>;

class Observer {
public:
    Observer(spsc_queue_shm<OrderBookUpdate>* queue, InstrumentID instrumentID)
        : queue_(queue), instrumentID_(instrumentID) {}
    ~Observer() = default;

    void popFromQueue();

private:
    Book& getBook_(OrderSide side);
    bool priceBetterOrEqual_(Price incoming, Price resting, OrderSide side);

    void addAtPrice_(Price price, Qty amount, OrderSide side);
    void reduceAtPrice_(Price price, Qty amount, OrderSide side);

    spsc_queue_shm<OrderBookUpdate>* queue_;
    InstrumentID instrumentID_;
    Book bids_;
    Book asks_;
};
