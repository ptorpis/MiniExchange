#include "core/matchingEngine.hpp"
#include "market-data/observer.hpp"
#include "utils/orderBuilder.hpp"
#include "utils/types.hpp"

#include "tests/observerTests.hpp"

#include <gtest/gtest.h>
#include <memory>

TEST_F(ObserverTest, LimitBuy) {
    auto buy = OrderBuilder{}.build();
    engine->processOrder(std::move(buy));
    observer->drainQueue();
    checkBooks(*engine, *observer);
}

TEST_F(ObserverTest, LimitSell) {
    auto sell = OrderBuilder{}.withSide(OrderSide::SELL).build();
    engine->processOrder(std::move(sell));
    observer->drainQueue();
    checkBooks(*engine, *observer);
}

TEST_F(ObserverTest, MarketBuyIntoEmpty) {
    auto marketBuy = OrderBuilder{}.withType(OrderType::MARKET).build();
    engine->processOrder(std::move(marketBuy));
    observer->drainQueue();
    checkBooks(*engine, *observer);
}

TEST_F(ObserverTest, MarketSellIntoEmpty) {
    auto marketBuy =
        OrderBuilder{}.withSide(OrderSide::SELL).withType(OrderType::MARKET).build();
    engine->processOrder(std::move(marketBuy));
    observer->drainQueue();
    checkBooks(*engine, *observer);
}

TEST_F(ObserverTest, PerfectFill) {
    auto buy = OrderBuilder{}.withClientID(ClientID{9}).build();
    auto sell = OrderBuilder{}.withSide(OrderSide::SELL).build();

    engine->processOrder(std::move(buy));
    engine->processOrder(std::move(sell));
    observer->drainQueue();

    checkBooks(*engine, *observer);
}

TEST_F(ObserverTest, NoCross) {
    auto buy = OrderBuilder{}.withClientID(ClientID{9}).build();
    auto sell = OrderBuilder{}.withSide(OrderSide::SELL).withPrice(Price{2001}).build();

    engine->processOrder(std::move(buy));
    engine->processOrder(std::move(sell));

    observer->drainQueue();

    checkBooks(*engine, *observer);
}

TEST_F(ObserverTest, PartialFillLimit) {
    auto buy = OrderBuilder{}.build();
    auto sell = OrderBuilder{}
                    .withClientID(OrderBuilder::Defaults::clientID + ClientID{1})
                    .withSide(OrderSide::SELL)
                    .withQty(OrderBuilder::Defaults::qty - Qty{1})
                    .build();

    engine->processOrder(std::move(buy));
    engine->processOrder(std::move(sell));
    observer->drainQueue();

    checkBooks(*engine, *observer);
}
