#include "market-data/observer.hpp"
#include "market-data/bookEvent.hpp"
#include "utils/types.hpp"
#include <cassert>

using namespace market_data;

auto& Observer::getBook_(OrderSide side) {
    return side == OrderSide::BUY ? book_.bids : book_.asks;
}

bool Observer::priceBetterOrEqual_(Price incoming, Price resting, OrderSide side) {
    if (side == OrderSide::BUY) {
        return incoming >= resting;
    } else {
        return resting >= incoming;
    }
}

void Observer::addAtPrice_(Price price, Qty amount, OrderSide side) {
    auto& book = getBook_(side);

    auto it = std::find_if(book.rbegin(), book.rend(), [&](const auto& level) {
        return level.first == price || !priceBetterOrEqual_(price, level.first, side);
    });

    if (it != book.rend() && it->first == price) {
        it->second += amount;
        return;
    }

    auto insertIt = (it == book.rend()) ? book.begin() : it.base();
    book.insert(insertIt, {price, amount});
}

void Observer::reduceAtPrice_(Price price, Qty amount, OrderSide side) {
    auto& book = getBook_(side);

    for (auto rIt = book.rbegin(); rIt != book.rend(); ++rIt) {
        if (rIt->first == price) {
            rIt->second -= amount;

            if (rIt->second == 0) {
                book.erase(std::next(rIt).base());
            }
            return;
        }
    }

#ifndef NDEBUG
    std::cerr << "price=" << price << " amount " << amount << " side " << side << "\n";
    assert(false && "Price not found");
#endif
}

void Observer::drainQueue() {
    OrderBookUpdate ev{};
    if (!engineQueue_) {
        return;
    }

    while (engineQueue_->try_pop(ev)) {
        if (ev.type == BookUpdateEventType::REDUCE) {
            reduceAtPrice_(ev.price, ev.amount, ev.side);
        } else {
            addAtPrice_(ev.price, ev.amount, ev.side);
        }
    }

    if (!mdQueue_) {
        return;
    }

    mdQueue_->try_push(ev);
}
