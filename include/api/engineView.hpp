#pragma once

#include "core/matchingEngine.hpp"
#include "utils/types.hpp"
class EngineView {
public:
    EngineView(const MatchingEngine& engine);

    std::optional<Price> getSpread() { return engine_.getSpread(); }
    std::optional<Price> getBestBid() { return engine_.getBestBid(); }
    std::optional<Price> getBestAsk() { return engine_.getBestAsk(); }

    std::optional<const Order*> getOrder(OrderID id) const;

    std::size_t getBidDepth() const;
    std::size_t getAskDepth() const;

private:
    const MatchingEngine& engine_;
};
