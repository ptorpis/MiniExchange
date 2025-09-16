#include "core/matchingEngine.hpp"
#include <gtest/gtest.h>

class MatchingEngineTest : public ::testing::Test {
protected:
    void SetUp() override { engine = std::make_unique<MatchingEngine>(); }

    std::unique_ptr<MatchingEngine> engine;
};

OrderRequest createTestMarketRequest(bool isBuy, Qty qty, Price price) {
    return OrderRequest{
        1,                                        // clientid
        isBuy ? OrderSide::BUY : OrderSide::SELL, // orderside
        OrderType::MARKET,                        // ordertype
        1,                                        // instrumentid
        qty,                                      // qty
        price,                                    // price
        TimeInForce::GTC,                         // timeinforce
        std::numeric_limits<Timestamp>::max(),    // goodtill
        true                                      // isvalid
    };
}

OrderRequest createTestLimitRequest(bool isBuy, Qty qty, Price price) {
    return OrderRequest{
        1,                                        // clientid
        isBuy ? OrderSide::BUY : OrderSide::SELL, // orderside
        OrderType::LIMIT,                         // ordertype
        1,                                        // instrumentid
        qty,                                      // qty
        price,                                    // price
        TimeInForce::GTC,                         // timeinforce
        std::numeric_limits<Timestamp>::max(),    // goodtill
        true                                      // isvalid
    };
}

TEST_F(MatchingEngineTest, EmptyBookHasNoSpread) {
    EXPECT_FALSE(engine->getBestAsk().has_value());
    EXPECT_FALSE(engine->getBestBid().has_value());
    EXPECT_FALSE(engine->getSpread().has_value());
}

TEST_F(MatchingEngineTest, MarketIntoEmptyBook) {
    OrderRequest mktreq = createTestMarketRequest(true, 100, 200);

    MatchResult result = engine->processOrder(mktreq);

    EXPECT_EQ(result.tradeVec.size(), 0);
    EXPECT_EQ(result.status, OrderStatus::CANCELLED);
}

TEST_F(MatchingEngineTest, LimitBuy) {
    OrderRequest limitReq = createTestLimitRequest(true, 100, 200);
    MatchResult result = engine->processOrder(limitReq);
    EXPECT_EQ(result.tradeVec.size(), 0);
    EXPECT_EQ(result.status, OrderStatus::NEW);
    EXPECT_EQ(engine->getBidsSize(), 1);
    EXPECT_EQ(engine->getAskSize(), 0);
}

TEST_F(MatchingEngineTest, LimitSell) {
    OrderRequest limitReq = createTestLimitRequest(false, 100, 200);
    MatchResult result = engine->processOrder(limitReq);
    EXPECT_EQ(result.tradeVec.size(), 0);
    EXPECT_EQ(result.status, OrderStatus::NEW);
    EXPECT_EQ(engine->getBidsSize(), 0);
    EXPECT_EQ(engine->getAskSize(), 1);
}

TEST_F(MatchingEngineTest, TestPerfectFill) {
    Price price = 200;
    Qty qty = 100;
    OrderRequest buyReq = createTestLimitRequest(true, qty, price);
    OrderRequest sellReq = createTestLimitRequest(false, qty, price);

    engine->processOrder(buyReq);
    MatchResult result = engine->processOrder(sellReq);

    EXPECT_EQ(result.tradeVec.size(), 1);
    EXPECT_EQ(result.tradeVec.at(0).price, price);
    EXPECT_EQ(result.tradeVec.at(0).qty, qty);
    EXPECT_EQ(engine->getBidsSize(), 0);
    EXPECT_EQ(engine->getAskSize(), 0);
}

TEST_F(MatchingEngineTest, OrderCancellation) {
    OrderRequest limitReq = createTestLimitRequest(true, 100, 200);
    MatchResult result = engine->processOrder(limitReq);
    OrderID orderID = result.orderID;

    EXPECT_EQ(engine->getBidsSize(), 1);

    EXPECT_TRUE(engine->cancelOrder(1, orderID));
    EXPECT_EQ(engine->getBidsSize(), 0);
}

TEST_F(MatchingEngineTest, FIFOAtPriceLevel) {
    OrderRequest req1 = createTestLimitRequest(true, 1, 200);
    OrderRequest req2 = createTestLimitRequest(true, 1, 200);
    MatchResult result1 = engine->processOrder(req1);
    MatchResult result2 = engine->processOrder(req2);
    EXPECT_EQ(engine->getBidsPriceLevelSize(200), 2);

    OrderID firstOrderID = result1.orderID;
    OrderID secondOrderID = result2.orderID;

    EXPECT_EQ(result2.tradeVec.size(), 0);

    OrderRequest firstFiller = createTestLimitRequest(false, 1, 200);

    MatchResult fillResult = engine->processOrder(firstFiller);

    EXPECT_EQ(fillResult.tradeVec.size(), 1);
    EXPECT_EQ(fillResult.tradeVec.at(0).buyerOrderID, firstOrderID);
    EXPECT_EQ(engine->getBidsPriceLevelSize(200), 1);

    OrderRequest secondFiller = createTestLimitRequest(false, 1, 200);

    MatchResult secondFillResult = engine->processOrder(secondFiller);

    EXPECT_EQ(engine->getBidsSize(), 0);
    EXPECT_EQ(secondFillResult.tradeVec.at(0).buyerOrderID, secondOrderID);
}

TEST_F(MatchingEngineTest, PartialFill) {
    OrderRequest req = createTestLimitRequest(true, 10, 200);
    MatchResult result1 = engine->processOrder(req);
    OrderID orderID = result1.orderID;

    OrderRequest req2 = createTestLimitRequest(false, 5, 200);
    MatchResult result2 = engine->processOrder(req2);
    EXPECT_EQ(result2.tradeVec.at(0).qty, 5);

    std::optional<const Order*> restingOrder = engine->getOrder(orderID);
    EXPECT_TRUE(restingOrder.has_value());
    EXPECT_EQ(restingOrder.value()->qty, 5);
    EXPECT_EQ(restingOrder.value()->status, OrderStatus::PARTIALLY_FILLED);
}

TEST_F(MatchingEngineTest, WalkTheBook) {
    OrderRequest req1 = createTestLimitRequest(true, 1, 200);
    OrderRequest req2 = createTestLimitRequest(true, 1, 201);
    OrderRequest req3 = createTestLimitRequest(true, 1, 202);

    engine->processOrder(req1);
    engine->processOrder(req2);
    engine->processOrder(req3);

    OrderRequest fillerRequest = createTestLimitRequest(false, 4, 202);

    MatchResult fillResult = engine->processOrder(fillerRequest);

    EXPECT_EQ(fillResult.tradeVec.size(), 3);
    std::optional<const Order*> restingOrder = engine->getOrder(fillResult.orderID);
    EXPECT_TRUE(restingOrder.has_value());
    EXPECT_EQ(restingOrder.value()->qty, 1);
}