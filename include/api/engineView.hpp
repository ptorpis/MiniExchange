#pragma once

#include "core/matchingEngine.hpp"
#include "utils/types.hpp"
class EngineView {
public:
    EngineView(const MatchingEngine& engine);

    std::optional<Price> getSpread();
    std::optional<Price> getBestBid();
    std::optional<Price> getBestAsk();

    std::optional<const Order*> getOrder(OrderID id) const;

    std::size_t getBidDepth() const;
    std::size_t getAskDepth() const;

private:
    const MatchingEngine& engine_;
};
