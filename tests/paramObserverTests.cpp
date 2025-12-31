#include "tests/observerTests.hpp"
#include "utils/orderBuilder.hpp"
#include <functional>
#include <random>
#include <string>
#include <vector>

using OrderScript = std::function<std::unique_ptr<Order>()>;
using OrderScenario = std::vector<OrderScript>;

struct RandomScenario {
    const char* name;
    OrderScenario orders;
};

class ObserverRandomTest
    : public ObserverTest,
      public ::testing::WithParamInterface<std::pair<const char*, uint64_t>> {};

TEST_P(ObserverRandomTest, EngineObserverParityRandom) {
    const auto& [name, seed] = GetParam();

    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> sideDist(0, 1); // 0 = BUY, 1 = SELL
    std::uniform_int_distribution<int> priceDist(100, 1'000);
    std::uniform_int_distribution<int> qtyDist(1, 1'000'000);

    OrderScenario orders;

    constexpr int kNumOrders = 1000; // Number of random orders per scenario

    for (int i = 0; i < kNumOrders; ++i) {
        bool isBuy = sideDist(rng) == 0;

        orders.push_back([=]() mutable {
            auto builder =
                OrderBuilder{}
                    .withQty(Qty{static_cast<unsigned long>(qtyDist(rng))})
                    .withPrice(Price{static_cast<unsigned long>(priceDist(rng))})
                    .withSide(isBuy ? OrderSide::BUY : OrderSide::SELL)
                    .withClientID(isBuy ? OrderBuilder::Defaults::clientID
                                        : OrderBuilder::Defaults::clientID + ClientID{1});
            return builder.build();
        });
    }

    for (auto& makeOrder : orders) {
        engine->processOrder(makeOrder());
        observer->drainQueue();
    }

    checkBooks(*engine, *observer);
}

// Seeds for reproducibility
static const std::vector<std::pair<const char*, uint64_t>> kRandomSeeds = {
    {"RandomScenario1", 42},
    {"RandomScenario2", 123456},
    {"RandomScenario3", 2025},
};

static std::string RandomScenarioName(
    const ::testing::TestParamInfo<std::pair<const char*, uint64_t>>& info) {
    return info.param.first;
}

INSTANTIATE_TEST_SUITE_P(ObserverParityRandom, ObserverRandomTest,
                         ::testing::ValuesIn(kRandomSeeds), RandomScenarioName);
