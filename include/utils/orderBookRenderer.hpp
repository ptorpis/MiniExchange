#pragma once
#include "core/matchingEngine.hpp"

#include <iomanip>
#include <iostream>

namespace utils {
class OrderBookRenderer {
public:
    inline static bool enabled = true;

    static void render(std::map<Price, Qty, std::greater<Price>> bids,
                       std::map<Price, Qty, std::less<Price>> asks, int depth = 15) {
        if (!enabled) return;

        std::cout << "\033[2J\033[1;1H"; // clear screen
        std::cout << "=============== ORDER BOOK ===============\n";
        std::cout << "   BID (Qty@Price)   |   ASK (Qty@Price)\n";
        std::cout << "---------------------+--------------------\n";

        auto bidIt = bids.begin();
        auto askIt = asks.begin();

        for (int i = 0; i < depth; ++i) {
            std::string bidStr = (bidIt != bids.end())
                                     ? (std::to_string(bidIt->second) + " @ " +
                                        std::to_string(bidIt->first))
                                     : "";
            std::string askStr = (askIt != asks.end())
                                     ? (std::to_string(askIt->second) + " @ " +
                                        std::to_string(askIt->first))
                                     : "";

            std::cout << std::setw(20) << bidStr << " | " << askStr << "\n";

            if (bidIt != bids.end()) ++bidIt;
            if (askIt != asks.end()) ++askIt;
        }
        std::cout << std::flush;
    }
};

} // namespace utils