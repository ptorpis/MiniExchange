#pragma once

#include "utils/types.hpp"

#include "core/matchingEngine.hpp"
#include "market-data/bookEvent.hpp"
#include "market-data/observer.hpp"
#include "utils/types.hpp"
#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <gtest/gtest.h>
#include <memory>
#include <print>
#include <ranges>
#include <stdexcept>

class ObserverTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::size_t qCap = 1023;
        std::size_t rawSize = sizeof(utils::spsc_queue_shm<L2OrderBookUpdate>) +
                              sizeof(L2OrderBookUpdate) * std::bit_ceil(qCap + 1);

        rawMem_ = std::malloc(rawSize);
        if (!rawMem_) {
            throw std::runtime_error("Malloc failed");
        }

        auto* queue =
            reinterpret_cast<utils::spsc_queue_shm<L2OrderBookUpdate>*>(rawMem_);
        queue->init(qCap);
        InstrumentID instrumentID{1};

        engine = std::make_unique<MatchingEngine>(queue, instrumentID);
        observer = std::make_unique<market_data::Observer>(queue, nullptr, l2b, l3b,
                                                           instrumentID);
    }

    std::unique_ptr<MatchingEngine> engine;
    std::unique_ptr<market_data::Observer> observer;
    Level2OrderBook l2b;
    Level3OrderBook l3b;
    void* rawMem_ = nullptr;

    void TearDown() override { std::free(rawMem_); }
};

inline void printBooks(const MatchingEngine& engine,
                       const market_data::Observer& observer) {
    std::println("----- BUYS -----");
    std::println("Matching Engine OrderBook Snapshot");

    auto bidsSnapshotME = engine.getSnapshot<OrderSide::BUY>();

    for (const auto& [price, amount] : bidsSnapshotME) {
        std::println("Level={} -- Amount={}", price.value(), amount.value());
    }

    std::println("Observer Snapshot");

    auto bidsSnapshotObserver = observer.getSnapshot<OrderSide::BUY>();

    for (const auto& [price, amount] : bidsSnapshotObserver) {
        std::println("Level={} -- Amount={}", price.value(), amount.value());
    }

    std::println("----- SELLS -----");
    std::println("Matching Engine OrderBook Snapshot");

    auto asksSnapshotME = engine.getSnapshot<OrderSide::SELL>();

    for (const auto& [price, amount] : asksSnapshotME) {
        std::println("Level={} -- Amount={}", price.value(), amount.value());
    }

    std::println("Observer Snapshot");

    auto asksSnapshotObserver = observer.getSnapshot<OrderSide::SELL>();

    for (const auto& [price, amount] : asksSnapshotObserver) {
        std::println("Level={} -- Amount={}", price.value(), amount.value());
    }
}

template <OrderSide Side>
void checkSide(const MatchingEngine& engine, const market_data::Observer& observer) {
    auto engineSnapshot = engine.getSnapshot<Side>();

    auto observerSnapshot = observer.getSnapshot<Side>();
    if constexpr (Side == OrderSide::BUY) {
        std::ranges::reverse(observerSnapshot);
    } else {
        std::ranges::reverse(engineSnapshot);
    }

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
        printBooks(engine, observer);
        throw std::runtime_error(Side == OrderSide::BUY ? "BUY book mismatch"
                                                        : "SELL book mismatch");
    }
}

inline void checkBooks(const MatchingEngine& engine,
                       const market_data::Observer& observer) {
    static std::size_t iterCount{};
    try {
        ++iterCount;
        checkSide<OrderSide::BUY>(engine, observer);
        checkSide<OrderSide::SELL>(engine, observer);
    } catch (...) {
        std::cerr << "iter count=" << iterCount << "\n";
        throw;
    }
}
