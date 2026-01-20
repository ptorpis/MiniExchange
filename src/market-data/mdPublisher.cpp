#include "market-data/MDPublisher.hpp"
#include "market-data/bookEvent.hpp"
#include "market-data/serialization.hpp"
#include "utils/spsc_queue.hpp"
#include "utils/types.hpp"
#include "utils/utils.hpp"
#include <chrono>
#include <exception>

using namespace market_data;

MarketDataPublisher::MarketDataPublisher(utils::spsc_queue<L2OrderBookUpdate>* queue,
                                         const Level2OrderBook& book,
                                         InstrumentID instrumentID,
                                         PublisherConfig cfg = PublisherConfig{})
    : queue_(queue), book_(book), instrumentID_(instrumentID), cfg_(cfg), msgSqn_(0),
      lastSnapshot_(std::chrono::steady_clock::now()), transport_() {}

void MarketDataPublisher::runOnce() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSnapshot_);

    if (elapsed >= cfg_.snapShotInterval) {
        publishSnapshot();
        lastSnapshot_ = now;
    }

    publishDelta();
}

void MarketDataPublisher::publishSnapshot() {
    if (!queue_) {
        return;
    }
    auto message = serializeSnapshotMessage(msgSqn_++, instrumentID_.value(), book_.bids,
                                            book_.asks, cfg_.maxDepth);
    sendPacket_(message);
}

void MarketDataPublisher::publishDelta() {
    if (!queue_) {
        return;
    }

    L2OrderBookUpdate update{};
    while (queue_->try_pop(update)) {
        DeltaPayload delta{};
        delta.priceLevel = update.price.value();
        delta.amountDelta = update.amount.value();
        delta.deltaType = +update.type;
        delta.side = +update.side;
        std::memset(delta._padding, 0, sizeof(delta._padding));

        auto message = serializeDeltaMessage(msgSqn_++, instrumentID_.value(), delta);

        sendPacket_(std::span<const std::byte>(message.data(), message.size()));
    }
}

void MarketDataPublisher::sendPacket_(std::span<const std::byte> msgBytes) {
    utils::printHex(msgBytes);
    try {
        transport_.send(msgBytes);
    } catch (const std::exception& e) {
        std::cerr << "Failed to send market data packet " << e.what() << "\n";
    }
}
