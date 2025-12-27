#include <gtest/gtest.h>
#include <memory>
#include <utility>

#include "core/matchingEngine.hpp"
#include "utils/types.hpp"

class MatchingEngineTest : public ::testing::Test {
protected:
    void SetUp() override { engine = std::make_unique<MatchingEngine>(); }

    std::unique_ptr<MatchingEngine> engine;
};

struct OrderBuilder {

    struct Defaults {
        static constexpr OrderID orderID{1};
        static constexpr ClientID clientID{1};
        static constexpr ClientOrderID clientOrderID{3};
        static constexpr Qty qty = Qty{100};
        static constexpr Price price{2000};
        static constexpr Timestamp goodTill{0};
        static constexpr Timestamp timestamp{0};
        static constexpr InstrumentID instrumentID{1};
        static constexpr TimeInForce tif{TimeInForce::GOOD_TILL_CANCELLED};
        static constexpr OrderSide side{OrderSide::BUY};
        static constexpr OrderType type{OrderType::LIMIT};
        static constexpr OrderStatus status{OrderStatus::NEW};
    };

    OrderID orderID{Defaults::orderID};
    ClientID clientID{Defaults::clientID};
    ClientOrderID clientOrderID{Defaults::clientOrderID};
    Qty qty{Defaults::qty};
    Price price{Defaults::price};
    Timestamp goodTill{Defaults::goodTill};
    Timestamp timestamp{Defaults::timestamp};
    InstrumentID instrumentID{Defaults::instrumentID};
    TimeInForce tif{Defaults::tif};
    OrderSide side{Defaults::side};
    OrderType type{Defaults::type};
    OrderStatus status{Defaults::status};

    OrderBuilder& withOrderID(OrderID id) {
        orderID = id;
        return *this;
    }
    OrderBuilder& withClientID(ClientID id) {
        clientID = id;
        return *this;
    }
    OrderBuilder& withClientOrderID(ClientOrderID id) {
        clientOrderID = id;
        return *this;
    }
    OrderBuilder& withQty(Qty q) {
        qty = q;
        return *this;
    }
    OrderBuilder& withPrice(Price p) {
        price = p;
        return *this;
    }
    OrderBuilder& withGoodTill(Timestamp t) {
        goodTill = t;
        return *this;
    }
    OrderBuilder& withTimestamp(Timestamp t) {
        timestamp = t;
        return *this;
    }
    OrderBuilder& withInstrumentID(InstrumentID id) {
        instrumentID = id;
        return *this;
    }
    OrderBuilder& withTIF(TimeInForce t) {
        tif = t;
        return *this;
    }
    OrderBuilder& withSide(OrderSide s) {
        side = s;
        return *this;
    }
    OrderBuilder& withType(OrderType t) {
        type = t;
        return *this;
    }
    OrderBuilder& withStatus(OrderStatus s) {
        status = s;
        return *this;
    }

    std::unique_ptr<Order> build() {
        return std::make_unique<Order>(orderID, clientID, clientOrderID, qty, price,
                                       goodTill, timestamp, instrumentID, tif, side, type,
                                       status);
    }
};

TEST_F(MatchingEngineTest, EmptyBookHasNoAsk) {
    EXPECT_FALSE(engine->getBestAsk().has_value());
}

TEST_F(MatchingEngineTest, EmptyBookHasNoBid) {
    EXPECT_FALSE(engine->getBestBid().has_value());
}

TEST_F(MatchingEngineTest, EmptyBookHasNoSpread) {
    EXPECT_FALSE(engine->getSpread().has_value());
}

TEST_F(MatchingEngineTest, MarketOrderIntoEmptyBook) {
    auto order1 = OrderBuilder{}.withType(OrderType::MARKET).build();

    MatchResult res = engine->processOrder(std::move(order1));

    EXPECT_EQ(res.tradeVec.size(), 0);
    EXPECT_EQ(res.status, OrderStatus::CANCELLED);
    EXPECT_FALSE(engine->getBestBid().has_value());
    EXPECT_FALSE(engine->getBestAsk().has_value());
    EXPECT_FALSE(engine->getSpread().has_value());
}

TEST_F(MatchingEngineTest, LimitBuy) {
    auto limitOrder = OrderBuilder{}.build();
    MatchResult res = engine->processOrder(std::move(limitOrder));
    EXPECT_EQ(res.tradeVec.size(), 0);
    EXPECT_EQ(res.status, OrderStatus::NEW);

    EXPECT_EQ(engine->getBestBid()->value(), OrderBuilder::Defaults::price);

    EXPECT_FALSE(engine->getBestAsk().has_value());
    EXPECT_FALSE(engine->getSpread().has_value());
}

TEST_F(MatchingEngineTest, LimitSell) {
    auto limitOrder = OrderBuilder{}.withSide(OrderSide::SELL).build();
    MatchResult res = engine->processOrder(std::move(limitOrder));
    EXPECT_EQ(res.tradeVec.size(), 0);
    EXPECT_EQ(res.status, OrderStatus::NEW);

    EXPECT_EQ(engine->getBestAsk()->value(), OrderBuilder::Defaults::price);

    EXPECT_FALSE(engine->getBestBid().has_value());
    EXPECT_FALSE(engine->getSpread().has_value());
}

TEST_F(MatchingEngineTest, PerfectFill) {
    auto buy = OrderBuilder{}.withClientID(ClientID{9}).build();
    auto sell = OrderBuilder{}.withSide(OrderSide::SELL).build();

    engine->processOrder(std::move(buy));
    auto res = engine->processOrder(std::move(sell));
    EXPECT_EQ(res.tradeVec.size(), 1);

    EXPECT_FALSE(engine->getBestBid().has_value());
    EXPECT_FALSE(engine->getBestAsk().has_value());
    EXPECT_EQ(res.status, OrderStatus::FILLED);

    EXPECT_EQ(res.tradeVec.at(0).price, OrderBuilder::Defaults::price);
    EXPECT_EQ(res.tradeVec.at(0).qty, OrderBuilder::Defaults::qty);
}

TEST_F(MatchingEngineTest, NoCross) {
    auto buy = OrderBuilder{}.withClientID(ClientID{9}).build();
    auto sell = OrderBuilder{}.withSide(OrderSide::SELL).withPrice(Price{2001}).build();

    engine->processOrder(std::move(buy));
    auto res = engine->processOrder(std::move(sell));

    EXPECT_EQ(res.tradeVec.size(), 0);
    EXPECT_TRUE(engine->getSpread().has_value());
    EXPECT_EQ(engine->getSpread()->value(), Price{1});
}

TEST_F(MatchingEngineTest, PartialFillLimit) {
    auto buy = OrderBuilder{}.build();
    auto sell = OrderBuilder{}
                    .withClientID(OrderBuilder::Defaults::clientID + ClientID{1})
                    .withSide(OrderSide::SELL)
                    .withQty(OrderBuilder::Defaults::qty - Qty{1})
                    .build();

    engine->processOrder(std::move(buy));
    auto res = engine->processOrder(std::move(sell));
    EXPECT_EQ(res.tradeVec.size(), 1);
    EXPECT_EQ(res.tradeVec.at(0).qty, OrderBuilder::Defaults::qty - Qty{1});
    EXPECT_TRUE(engine->getBestBid().has_value());
}

TEST_F(MatchingEngineTest, SellWalksTheBook) {
    auto buy1 = OrderBuilder{}.withPrice(Price{100}).withQty(Qty{10}).build();
    auto buy2 = OrderBuilder{}.withPrice(Price{101}).withQty(Qty{10}).build();
    auto buy3 = OrderBuilder{}.withPrice(Price{102}).withQty(Qty{10}).build();
    engine->processOrder(std::move(buy1));
    engine->processOrder(std::move(buy2));
    engine->processOrder(std::move(buy3));

    auto bigSell = OrderBuilder{}
                       .withOrderID(OrderID{999})
                       .withQty(Qty{40})
                       .withClientID(ClientID{9})
                       .withPrice(Price{100})
                       .withSide(OrderSide::SELL)
                       .build();

    auto res = engine->processOrder(std::move(bigSell));

    EXPECT_EQ(res.tradeVec.size(), 3);
    auto restingOrder = engine->getOrder(OrderID{999});
    EXPECT_TRUE(engine->getBestAsk().has_value());
    // EXPECT_EQ(restingOrder->qty, Qty{10});
    EXPECT_NE(restingOrder, nullptr);
}
