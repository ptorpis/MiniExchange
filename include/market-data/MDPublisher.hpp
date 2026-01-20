#pragma once

#include "market-data/bookEvent.hpp"
#include "market-data/udpMulticastTransport.hpp"
#include "utils/spsc_queue.hpp"
#include "utils/types.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace market_data {

struct PublisherConfig {
    std::size_t maxDepth{64};
    std::chrono::milliseconds snapShotInterval{1000};
};

struct MarketDataPublisher {
public:
    MarketDataPublisher(utils::spsc_queue<L2OrderBookUpdate>* queue,
                        const Level2OrderBook& book, InstrumentID instrumentID,
                        PublisherConfig cfg);

    void runOnce();
    void publishSnapshot();
    void publishDelta();

private:
    void sendPacket_(std::span<const std::byte> messageBytes);

    utils::spsc_queue<L2OrderBookUpdate>* queue_;
    const Level2OrderBook& book_;
    InstrumentID instrumentID_;
    PublisherConfig cfg_;

    std::uint64_t msgSqn_{0};
    std::chrono::steady_clock::time_point lastSnapshot_;

    market_data::UDPMulticastTransport transport_;
};
} // namespace market_data
