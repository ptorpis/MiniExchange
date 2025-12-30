#include "market-data/observer.hpp"
#include "market-data/bookEvent.hpp"
#include "utils/types.hpp"
#include <cassert>

Book& Observer::getBook_(OrderSide side) {
    return side == OrderSide::BUY ? bids_ : asks_;
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

    std::cout << "ADD: " << amount << " @ " << price << " on " << side << "\n";

    for (auto rIt = book.rbegin(); rIt != book.rend(); ++rIt) {
        if (rIt->first == price) {
            rIt->second += amount;
            return;
        }

        if (!priceBetterOrEqual_(price, rIt->first, side)) {
            book.insert(rIt.base(), {price, amount});
        }
    }

    book.insert(book.begin(), {price, amount});
}

void Observer::reduceAtPrice_(Price price, Qty amount, OrderSide side) {
    auto& book = getBook_(side);

    std::cout << "REDUCE: " << amount << " @ " << price << " on " << side << "\n";

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

void Observer::popFromQueue() {
    OrderBookUpdate ev{};
    if (!queue_) {
        return;
    }

    while (queue_->try_pop(ev)) {
        if (ev.type == BookUpdateEventType::REDUCE) {
            reduceAtPrice_(ev.price, ev.amount, ev.side);
        } else {
            addAtPrice_(ev.price, ev.amount, ev.side);
        }
    }
}
