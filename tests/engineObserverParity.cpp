#include "core/matchingEngine.hpp"
#include "market-data/bookEvent.hpp"
#include "market-data/observer.hpp"
#include "utils/orderBuilder.hpp"
#include "utils/types.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdlib>
#include <gtest/gtest.h>
#include <memory>
#include <ranges>
#include <stdexcept>

class ObserverTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::size_t qCap = 1023;
        std::size_t rawSize = sizeof(utils::spsc_queue_shm<OrderBookUpdate>) +
                              sizeof(OrderBookUpdate) * std::bit_ceil(qCap + 1);

        rawMem_ = std::malloc(rawSize);
        if (!rawMem_) {
            throw std::runtime_error("Malloc failed");
        }

        auto* queue = reinterpret_cast<utils::spsc_queue_shm<OrderBookUpdate>*>(rawMem_);
        queue->init(qCap);
        InstrumentID instrumentID{1};

        engine = std::make_unique<MatchingEngine>(queue, instrumentID);
        observer = std::make_unique<Observer>(queue, instrumentID);
    }

    std::unique_ptr<MatchingEngine> engine;
    std::unique_ptr<Observer> observer;
    void* rawMem_ = nullptr;

    void TearDown() override { std::free(rawMem_); }
};

template <OrderSide Side>
void checkSide(const MatchingEngine& engine, const Observer& observer) {
    auto engineSnapshot = engine.getSnapshot<Side>();
    auto observerSnapshot = observer.getSnapshot<Side>();

    if (engineSnapshot.size() != observerSnapshot.size()) {
        throw std::runtime_error(Side == OrderSide::BUY ? "BUY size mismatch"
                                                        : "SELL size mismatch");
    }

    bool equal =
        std::ranges::equal(engineSnapshot | std::views::transform([](const auto& e) {
                               return std::make_pair(e.first, e.second);
                           }),
                           observerSnapshot, [](const auto& a, const auto& b) {
                               return a.first == b.first && a.second == b.second;
                           });

    if (!equal) {
        throw std::runtime_error(Side == OrderSide::BUY ? "BUY book mismatch"
                                                        : "SELL book mismatch");
    }
}

inline void checkBooks(const MatchingEngine& engine, const Observer& observer) {
    checkSide<OrderSide::BUY>(engine, observer);
    checkSide<OrderSide::SELL>(engine, observer);
}

TEST_F(ObserverTest, LimitBuy) {
    auto buy = OrderBuilder{}.build();
    engine->processOrder(std::move(buy));
    observer->popFromQueue();
    checkBooks(*engine, *observer);
}

TEST_F(ObserverTest, LimitSell) {
    auto sell = OrderBuilder{}.withSide(OrderSide::SELL).build();
    engine->processOrder(std::move(sell));
    observer->popFromQueue();
    checkBooks(*engine, *observer);
}
