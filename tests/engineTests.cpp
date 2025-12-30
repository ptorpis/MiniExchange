#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
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
    EXPECT_NE(restingOrder, nullptr);
    EXPECT_EQ(restingOrder->qty, Qty{10});
}

TEST_F(MatchingEngineTest, BuyWalksTheBook) {
    auto sell1 = OrderBuilder{}
                     .withPrice(Price{100})
                     .withQty(Qty{10})
                     .withSide(OrderSide::SELL)
                     .build();
    auto sell2 = OrderBuilder{}
                     .withPrice(Price{101})
                     .withQty(Qty{10})
                     .withSide(OrderSide::SELL)
                     .build();
    auto sell3 = OrderBuilder{}
                     .withPrice(Price{102})
                     .withQty(Qty{10})
                     .withSide(OrderSide::SELL)
                     .build();

    engine->processOrder(std::move(sell1));
    engine->processOrder(std::move(sell2));
    engine->processOrder(std::move(sell3));

    auto bigBuy = OrderBuilder{}
                      .withOrderID(OrderID{999})
                      .withQty(Qty{40})
                      .withClientID(ClientID{9})
                      .withPrice(Price{102})
                      .build();

    auto res = engine->processOrder(std::move(bigBuy));

    EXPECT_EQ(res.tradeVec.size(), 3);
    auto restingOrder = engine->getOrder(OrderID{999});
    EXPECT_TRUE(engine->getBestBid().has_value());
    EXPECT_NE(restingOrder, nullptr);
    EXPECT_EQ(restingOrder->qty, Qty{10});
}

TEST_F(MatchingEngineTest, WrongInstrumentID) {
    auto order = OrderBuilder{}.withInstrumentID(InstrumentID{2}).build();
    EXPECT_THROW(engine->processOrder(std::move(order)), std::runtime_error);
}

TEST_F(MatchingEngineTest, CancelOrder) {
    auto limitOrder = OrderBuilder{}.build();
    auto res = engine->processOrder(std::move(limitOrder));
    EXPECT_EQ(res.status, OrderStatus::NEW);

    auto cancelResult = engine->cancelOrder(ClientID{OrderBuilder::Defaults::clientID},
                                            OrderID{OrderBuilder::Defaults::orderID});
    EXPECT_TRUE(cancelResult);
    EXPECT_FALSE(engine->getBestBid().has_value());
}

TEST_F(MatchingEngineTest, CancelNonExistentOrder) {
    auto cancelResult = engine->cancelOrder(ClientID{1}, OrderID{999});
    EXPECT_FALSE(cancelResult);
}

TEST_F(MatchingEngineTest, ModifyPrice_Decrease) {
    auto limitOrder = OrderBuilder{}.withOrderID(OrderID{0}).build();
    auto res = engine->processOrder(std::move(limitOrder));
    EXPECT_EQ(res.status, OrderStatus::NEW);

    auto modifyResult =
        engine->modifyOrder(ClientID{OrderBuilder::Defaults::clientID}, OrderID{0},
                            Qty{OrderBuilder::Defaults::qty}, Price{1999});

    EXPECT_EQ(modifyResult.status, ModifyStatus::ACCEPTED);
    auto modifiedOrder = engine->getOrder(modifyResult.newOrderID);
    EXPECT_NE(modifiedOrder, nullptr);
    EXPECT_EQ(modifiedOrder->qty, Qty{OrderBuilder::Defaults::qty});
    EXPECT_EQ(modifiedOrder->price, Price{1999});
    EXPECT_NE(modifiedOrder->orderID, OrderID{0});
    EXPECT_TRUE(modifyResult.matchResult.has_value());
    ASSERT_EQ(engine->getBestBid()->value(), Price{1999});
    auto oldOrder = engine->getOrder(OrderID{0});
    EXPECT_EQ(oldOrder, nullptr);
}

TEST_F(MatchingEngineTest, ModifyPrice_Increase) {
    auto limitOrder = OrderBuilder{}.withOrderID(OrderID{0}).build();
    auto res = engine->processOrder(std::move(limitOrder));
    EXPECT_EQ(res.status, OrderStatus::NEW);

    auto modifyResult =
        engine->modifyOrder(ClientID{OrderBuilder::Defaults::clientID}, OrderID{0},
                            Qty{OrderBuilder::Defaults::qty}, Price{2001});

    EXPECT_EQ(modifyResult.status, ModifyStatus::ACCEPTED);
    auto modifiedOrder = engine->getOrder(modifyResult.newOrderID);
    EXPECT_NE(modifiedOrder, nullptr);
    EXPECT_EQ(modifiedOrder->qty, Qty{OrderBuilder::Defaults::qty});
    EXPECT_EQ(modifiedOrder->price, Price{2001});
    EXPECT_NE(modifiedOrder->orderID, OrderID{0});
    EXPECT_TRUE(modifyResult.matchResult.has_value());
    ASSERT_EQ(engine->getBestBid()->value(), Price{2001});
    auto oldOrder = engine->getOrder(OrderID{0});
    EXPECT_EQ(oldOrder, nullptr);
}

TEST_F(MatchingEngineTest, ModifyPrice_SamePrice) {
    auto limitOrder = OrderBuilder{}.withOrderID(OrderID{0}).build();
    auto res = engine->processOrder(std::move(limitOrder));
    EXPECT_EQ(res.status, OrderStatus::NEW);

    auto modifyResult = engine->modifyOrder(ClientID{OrderBuilder::Defaults::clientID},
                                            OrderID{0}, Qty{OrderBuilder::Defaults::qty},
                                            OrderBuilder::Defaults::price);

    EXPECT_EQ(modifyResult.status, ModifyStatus::ACCEPTED);
    auto modifiedOrder = engine->getOrder(modifyResult.newOrderID);
    EXPECT_NE(modifiedOrder, nullptr);
    EXPECT_EQ(modifiedOrder->qty, Qty{OrderBuilder::Defaults::qty});
    EXPECT_EQ(modifiedOrder->price, OrderBuilder::Defaults::price);
    EXPECT_EQ(modifyResult.oldOrderID, modifyResult.newOrderID);
    EXPECT_FALSE(modifyResult.matchResult.has_value());
    ASSERT_EQ(engine->getBestBid()->value(), OrderBuilder::Defaults::price);
}

TEST_F(MatchingEngineTest, ModifyInPlace_ReduceQty) {
    auto limitOrder = OrderBuilder{}.withOrderID(OrderID{0}).build();
    auto res = engine->processOrder(std::move(limitOrder));
    EXPECT_EQ(res.status, OrderStatus::NEW);

    auto modifyResult =
        engine->modifyOrder(ClientID{OrderBuilder::Defaults::clientID}, OrderID{0},
                            Qty{50}, Price{OrderBuilder::Defaults::price});

    EXPECT_EQ(modifyResult.status, ModifyStatus::ACCEPTED);
    auto modifiedOrder = engine->getOrder(modifyResult.newOrderID);
    EXPECT_NE(modifiedOrder, nullptr);
    EXPECT_EQ(modifiedOrder->qty, Qty{50});
    EXPECT_EQ(modifiedOrder->price, OrderBuilder::Defaults::price);
    EXPECT_EQ(modifiedOrder->orderID, OrderID{0});
    EXPECT_FALSE(modifyResult.matchResult.has_value());
    ASSERT_EQ(engine->getBestBid()->value(), OrderBuilder::Defaults::price);
}

TEST_F(MatchingEngineTest, ModifyOrder_IncreaseQty) {
    auto limitOrder = OrderBuilder{}.withOrderID(OrderID{0}).build();
    auto res = engine->processOrder(std::move(limitOrder));
    EXPECT_EQ(res.status, OrderStatus::NEW);

    auto modifyResult =
        engine->modifyOrder(ClientID{OrderBuilder::Defaults::clientID}, OrderID{0},
                            Qty{150}, Price{OrderBuilder::Defaults::price});

    EXPECT_EQ(modifyResult.status, ModifyStatus::ACCEPTED);
    auto modifiedOrder = engine->getOrder(modifyResult.newOrderID);
    EXPECT_NE(modifiedOrder, nullptr);
    EXPECT_EQ(modifiedOrder->qty, Qty{150});
    EXPECT_EQ(modifiedOrder->price, OrderBuilder::Defaults::price);
    EXPECT_NE(modifiedOrder->orderID, OrderID{0});
    EXPECT_TRUE(modifyResult.matchResult.has_value());
    ASSERT_EQ(engine->getBestBid()->value(), OrderBuilder::Defaults::price);
    auto oldOrder = engine->getOrder(OrderID{0});
    EXPECT_EQ(oldOrder, nullptr);
}

TEST_F(MatchingEngineTest, ModifyOrder_NotFound) {
    auto modifyResult =
        engine->modifyOrder(ClientID{1}, OrderID{999}, Qty{50}, Price{1999});

    EXPECT_EQ(modifyResult.status, ModifyStatus::NOT_FOUND);
    EXPECT_EQ(modifyResult.newOrderID, OrderID{0});
    EXPECT_FALSE(modifyResult.matchResult.has_value());
}

TEST_F(MatchingEngineTest, CancelOrder_WrongClientID) {
    auto limitOrder = OrderBuilder{}.build();
    auto res = engine->processOrder(std::move(limitOrder));
    EXPECT_EQ(res.status, OrderStatus::NEW);

    auto cancelResult =
        engine->cancelOrder(ClientID{999}, OrderID{OrderBuilder::Defaults::orderID});
    EXPECT_FALSE(cancelResult);
    EXPECT_TRUE(engine->getBestBid().has_value());
}

TEST_F(MatchingEngineTest, ModifyOrder_WrongClientID) {
    auto limitOrder = OrderBuilder{}.build();
    auto res = engine->processOrder(std::move(limitOrder));
    EXPECT_EQ(res.status, OrderStatus::NEW);

    auto modifyResult = engine->modifyOrder(
        ClientID{999}, OrderID{OrderBuilder::Defaults::orderID}, Qty{50}, Price{1999});

    EXPECT_EQ(modifyResult.status, ModifyStatus::INVALID);
    EXPECT_EQ(modifyResult.newOrderID, OrderID{0});
    EXPECT_FALSE(modifyResult.matchResult.has_value());
    auto oldOrder = engine->getOrder(OrderID{OrderBuilder::Defaults::orderID});
    EXPECT_NE(oldOrder, nullptr);
    EXPECT_EQ(oldOrder->qty, OrderBuilder::Defaults::qty);
    EXPECT_EQ(oldOrder->price, OrderBuilder::Defaults::price);
}

TEST_F(MatchingEngineTest, ModifyOrder_NoChange) {
    auto limitOrder = OrderBuilder{}.withOrderID(OrderID{0}).build();
    auto res = engine->processOrder(std::move(limitOrder));
    EXPECT_EQ(res.status, OrderStatus::NEW);

    auto modifyResult = engine->modifyOrder(ClientID{OrderBuilder::Defaults::clientID},
                                            OrderID{0}, Qty{OrderBuilder::Defaults::qty},
                                            Price{OrderBuilder::Defaults::price});

    EXPECT_EQ(modifyResult.status, ModifyStatus::ACCEPTED);
    EXPECT_EQ(modifyResult.newOrderID, OrderID{0});
    EXPECT_FALSE(modifyResult.matchResult.has_value());
    auto oldOrder = engine->getOrder(OrderID{0});
    EXPECT_NE(oldOrder, nullptr);
    EXPECT_EQ(oldOrder->qty, OrderBuilder::Defaults::qty);
    EXPECT_EQ(oldOrder->price, OrderBuilder::Defaults::price);
}

TEST_F(MatchingEngineTest, CancelOrder_Twice) {
    auto limitOrder = OrderBuilder{}.build();
    auto res = engine->processOrder(std::move(limitOrder));
    EXPECT_EQ(res.status, OrderStatus::NEW);

    auto cancelResult = engine->cancelOrder(ClientID{OrderBuilder::Defaults::clientID},
                                            OrderID{OrderBuilder::Defaults::orderID});
    EXPECT_TRUE(cancelResult);

    cancelResult = engine->cancelOrder(ClientID{OrderBuilder::Defaults::clientID},
                                       OrderID{OrderBuilder::Defaults::orderID});
    EXPECT_FALSE(cancelResult);
    EXPECT_FALSE(engine->getBestBid().has_value());
}

TEST_F(MatchingEngineTest, ModifyOrder_Twice) {
    auto limitOrder = OrderBuilder{}.withOrderID(OrderID{0}).build();
    auto res = engine->processOrder(std::move(limitOrder));
    EXPECT_EQ(res.status, OrderStatus::NEW);

    auto modifyResult = engine->modifyOrder(ClientID{OrderBuilder::Defaults::clientID},
                                            OrderID{0}, Qty{150}, Price{2001});

    EXPECT_EQ(modifyResult.status, ModifyStatus::ACCEPTED);
    auto modifiedOrder = engine->getOrder(modifyResult.newOrderID);
    EXPECT_NE(modifiedOrder, nullptr);
    EXPECT_EQ(modifiedOrder->qty, Qty{150});
    EXPECT_EQ(modifiedOrder->price, Price{2001});
    EXPECT_NE(modifiedOrder->orderID, OrderID{0});
    EXPECT_TRUE(modifyResult.matchResult.has_value());
    ASSERT_EQ(engine->getBestBid()->value(), Price{2001});
    auto oldOrder = engine->getOrder(OrderID{0});
    EXPECT_EQ(oldOrder, nullptr);

    auto secondModifyResult =
        engine->modifyOrder(ClientID{OrderBuilder::Defaults::clientID},
                            modifyResult.newOrderID, Qty{100}, Price{1999});

    EXPECT_EQ(secondModifyResult.status, ModifyStatus::ACCEPTED);
    auto secondModifiedOrder = engine->getOrder(secondModifyResult.newOrderID);
    EXPECT_NE(secondModifiedOrder, nullptr);
    EXPECT_EQ(secondModifiedOrder->qty, Qty{100});
    EXPECT_EQ(secondModifiedOrder->price, Price{1999});
    EXPECT_NE(secondModifiedOrder->orderID, modifyResult.newOrderID);
    EXPECT_TRUE(secondModifyResult.matchResult.has_value());
    ASSERT_EQ(engine->getBestBid()->value(), Price{1999});
    auto firstOldOrder = engine->getOrder(modifyResult.newOrderID);
    EXPECT_EQ(firstOldOrder, nullptr);
}

TEST_F(MatchingEngineTest, CancelOrder_AfterModify) {
    auto limitOrder = OrderBuilder{}.withOrderID(OrderID{0}).build();
    auto res = engine->processOrder(std::move(limitOrder));
    EXPECT_EQ(res.status, OrderStatus::NEW);

    auto modifyResult = engine->modifyOrder(ClientID{OrderBuilder::Defaults::clientID},
                                            OrderID{0}, Qty{150}, Price{2001});

    EXPECT_EQ(modifyResult.status, ModifyStatus::ACCEPTED);
    auto modifiedOrder = engine->getOrder(modifyResult.newOrderID);
    EXPECT_NE(modifiedOrder, nullptr);
    EXPECT_EQ(modifiedOrder->qty, Qty{150});
    EXPECT_EQ(modifiedOrder->price, Price{2001});
    EXPECT_NE(modifiedOrder->orderID, OrderID{0});
    EXPECT_TRUE(modifyResult.matchResult.has_value());
    ASSERT_EQ(engine->getBestBid()->value(), Price{2001});
    auto oldOrder = engine->getOrder(OrderID{0});
    EXPECT_EQ(oldOrder, nullptr);

    auto cancelResult = engine->cancelOrder(ClientID{OrderBuilder::Defaults::clientID},
                                            modifyResult.newOrderID);
    EXPECT_TRUE(cancelResult);
    EXPECT_FALSE(engine->getBestBid().has_value());
}

TEST_F(MatchingEngineTest, TryToFillAfterCancelled) {
    auto buyOrder = OrderBuilder{}.withOrderID(OrderID{1}).build();
    auto res = engine->processOrder(std::move(buyOrder));
    EXPECT_EQ(res.status, OrderStatus::NEW);

    auto cancelResult =
        engine->cancelOrder(ClientID{OrderBuilder::Defaults::clientID}, OrderID{1});
    EXPECT_TRUE(cancelResult);

    auto sellOrder =
        OrderBuilder{}.withOrderID(OrderID{2}).withSide(OrderSide::SELL).build();
    auto sellRes = engine->processOrder(std::move(sellOrder));
    EXPECT_EQ(sellRes.status, OrderStatus::NEW);
    EXPECT_TRUE(engine->getBestAsk().has_value());
    EXPECT_EQ(sellRes.tradeVec.size(), 0);
}

TEST_F(MatchingEngineTest, CancelOrder_WrongOrderID) {
    auto limitOrder = OrderBuilder{}.build();
    auto res = engine->processOrder(std::move(limitOrder));
    EXPECT_EQ(res.status, OrderStatus::NEW);

    auto cancelResult =
        engine->cancelOrder(ClientID{OrderBuilder::Defaults::clientID}, OrderID{999});
    EXPECT_FALSE(cancelResult);
    EXPECT_TRUE(engine->getBestBid().has_value());
}

TEST_F(MatchingEngineTest, ModifyOrder_WrongOrderID) {
    auto limitOrder = OrderBuilder{}.withOrderID(OrderID{0}).build();
    auto res = engine->processOrder(std::move(limitOrder));
    EXPECT_EQ(res.status, OrderStatus::NEW);

    auto modifyResult = engine->modifyOrder(ClientID{OrderBuilder::Defaults::clientID},
                                            OrderID{999}, Qty{50}, Price{1999});

    EXPECT_EQ(modifyResult.status, ModifyStatus::NOT_FOUND);
    EXPECT_EQ(modifyResult.newOrderID, OrderID{0});
    EXPECT_FALSE(modifyResult.matchResult.has_value());
    auto oldOrder = engine->getOrder(OrderID{0});
    EXPECT_NE(oldOrder, nullptr);
    EXPECT_EQ(oldOrder->qty, OrderBuilder::Defaults::qty);
    EXPECT_EQ(oldOrder->price, OrderBuilder::Defaults::price);
}

TEST_F(MatchingEngineTest, RestOfMarketOrderGetsCancelled) {
    auto sellLimit = OrderBuilder{}
                         .withOrderID(OrderID{1})
                         .withSide(OrderSide::SELL)
                         .withPrice(Price{2000})
                         .withQty(Qty{50})
                         .build();
    auto res1 = engine->processOrder(std::move(sellLimit));
    EXPECT_EQ(res1.status, OrderStatus::NEW);
    auto buyMarket = OrderBuilder{}
                         .withOrderID(OrderID{2})
                         .withSide(OrderSide::BUY)
                         .withType(OrderType::MARKET)
                         .withQty(Qty{100})
                         .withClientID(ClientID{9})
                         .build();
    auto res2 = engine->processOrder(std::move(buyMarket));
    EXPECT_EQ(res2.status, OrderStatus::PARTIALLY_FILLED);
    EXPECT_EQ(res2.tradeVec.size(), 1);
    EXPECT_EQ(res2.tradeVec.at(0).qty, Qty{50});
    EXPECT_FALSE(engine->getBestBid().has_value());
    EXPECT_FALSE(engine->getBestAsk().has_value());
}
