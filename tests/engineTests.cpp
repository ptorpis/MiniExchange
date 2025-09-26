#include "core/matchingEngine.hpp"
#include "utils/orderBookRenderer.hpp"
#include <gtest/gtest.h>

class MatchingEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        utils::OrderBookRenderer::enabled = false;
        engine = std::make_unique<MatchingEngine>();
    }

    std::unique_ptr<MatchingEngine> engine;
};

OrderRequest createTestMarketRequest(bool isBuy, Qty qty, Price price,
                                     ClientID clientID = 1) {
    return OrderRequest{
        clientID,                                 // clientid
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

OrderRequest createTestLimitRequest(bool isBuy, Qty qty, Price price,
                                    ClientID clientID = 1) {
    return OrderRequest{
        clientID,                                 // clientid
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
    OrderRequest sellReq = createTestLimitRequest(false, qty, price, 2);

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

    OrderRequest firstFiller = createTestLimitRequest(false, 1, 200, 2);

    MatchResult fillResult = engine->processOrder(firstFiller);

    EXPECT_EQ(fillResult.tradeVec.size(), 1);
    EXPECT_EQ(fillResult.tradeVec.at(0).buyerOrderID, firstOrderID);
    EXPECT_EQ(engine->getBidsPriceLevelSize(200), 1);

    OrderRequest secondFiller = createTestLimitRequest(false, 1, 200, 2);

    MatchResult secondFillResult = engine->processOrder(secondFiller);

    EXPECT_EQ(engine->getBidsSize(), 0);
    EXPECT_EQ(secondFillResult.tradeVec.at(0).buyerOrderID, secondOrderID);
}

TEST_F(MatchingEngineTest, PartialFill) {
    OrderRequest req = createTestLimitRequest(true, 10, 200);
    MatchResult result1 = engine->processOrder(req);
    OrderID orderID = result1.orderID;

    OrderRequest req2 = createTestLimitRequest(false, 5, 200, 2);
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

    OrderRequest fillerRequest = createTestLimitRequest(false, 4, 200, 2);

    MatchResult fillResult = engine->processOrder(fillerRequest);

    EXPECT_EQ(fillResult.tradeVec.size(), 3);
    std::optional<const Order*> restingOrder = engine->getOrder(fillResult.orderID);
    EXPECT_TRUE(restingOrder.has_value());
    EXPECT_EQ(restingOrder.value()->qty, 1);
}

TEST_F(MatchingEngineTest, CancelOrderNotFound) {
    EXPECT_FALSE(engine->cancelOrder(1, 9999));
}

TEST_F(MatchingEngineTest, ModifyOrderNotFound) {
    ModifyResult modRes = engine->modifyOrder(1, 9999, 100, 200);
    EXPECT_EQ(modRes.event.status, statusCodes::ModifyStatus::NOT_FOUND);
}

TEST_F(MatchingEngineTest, LimitDoesNotCross) {
    OrderRequest buyReq = createTestLimitRequest(true, 100, 199);
    MatchResult buyRes = engine->processOrder(buyReq);
    EXPECT_EQ(buyRes.status, OrderStatus::NEW);

    OrderRequest sellReq = createTestLimitRequest(false, 100, 201);
    MatchResult sellRes = engine->processOrder(sellReq);
    EXPECT_EQ(sellRes.status, OrderStatus::NEW);

    EXPECT_EQ(engine->getBidsSize(), 1);
    EXPECT_EQ(engine->getAskSize(), 1);
}

TEST_F(MatchingEngineTest, ModifyReduceQty) {
    OrderRequest req = createTestLimitRequest(true, 100, 200);
    MatchResult res = engine->processOrder(req);
    ModifyResult modRes = engine->modifyOrder(1, res.orderID, 99, 200);

    ASSERT_EQ(modRes.event.status, statusCodes::ModifyStatus::ACCEPTED);
    std::optional<const Order*> order = engine->getOrder(res.orderID);

    // check if the modification was in-place
    ASSERT_TRUE(order.has_value());
    ASSERT_EQ(modRes.event.newOrderID, modRes.event.oldOrderID);
    ASSERT_EQ(order.value()->qty, 99);
    ASSERT_EQ(order.value()->price, 200);
    ASSERT_EQ(order.value()->status, OrderStatus::MODIFIED);
}

TEST_F(MatchingEngineTest, ModifyIncreaseQty) {
    OrderRequest req = createTestLimitRequest(true, 100, 200);
    MatchResult res = engine->processOrder(req);
    ModifyResult modRes = engine->modifyOrder(1, res.orderID, 101, 200);

    ASSERT_EQ(modRes.event.status, statusCodes::ModifyStatus::ACCEPTED);
    ASSERT_NE(modRes.event.newOrderID, modRes.event.oldOrderID);
    std::optional<const Order*> order = engine->getOrder(modRes.event.newOrderID);

    // check if the old order still exists (should not)
    std::optional<const Order*> oldOrder = engine->getOrder(res.orderID);
    ASSERT_TRUE(order.has_value());
    ASSERT_FALSE(oldOrder.has_value());
    ASSERT_EQ(order.value()->qty, 101);
    ASSERT_EQ(order.value()->price, 200);
    ASSERT_EQ(order.value()->status, OrderStatus::MODIFIED);
}

TEST_F(MatchingEngineTest, ModifyChangePrice) {
    OrderRequest req = createTestLimitRequest(true, 100, 200);
    MatchResult res = engine->processOrder(req);
    ModifyResult modRes = engine->modifyOrder(1, res.orderID, 100, 250);

    ASSERT_EQ(modRes.event.status, statusCodes::ModifyStatus::ACCEPTED);
    ASSERT_NE(modRes.event.newOrderID, modRes.event.oldOrderID);
    std::optional<const Order*> order = engine->getOrder(modRes.event.newOrderID);

    // check if the old order still exists (should not)
    std::optional<const Order*> oldOrder = engine->getOrder(res.orderID);
    ASSERT_TRUE(order.has_value());
    ASSERT_FALSE(oldOrder.has_value());
    ASSERT_EQ(order.value()->qty, 100);
    ASSERT_EQ(order.value()->price, 250);
    ASSERT_EQ(order.value()->status, OrderStatus::MODIFIED);
}

TEST_F(MatchingEngineTest, ModifyInvalidClient) {
    OrderRequest req = createTestLimitRequest(true, 100, 200);
    MatchResult res = engine->processOrder(req);
    ModifyResult modRes = engine->modifyOrder(2, res.orderID, 100, 250);

    ASSERT_EQ(modRes.event.status, statusCodes::ModifyStatus::INVALID);
    std::optional<const Order*> order = engine->getOrder(res.orderID);

    ASSERT_TRUE(order.has_value());
    ASSERT_EQ(order.value()->qty, 100);
    ASSERT_EQ(order.value()->price, 200);
    ASSERT_EQ(order.value()->status, OrderStatus::NEW);
}

TEST_F(MatchingEngineTest, ModifyToCross) {
    OrderRequest buyReq = createTestLimitRequest(true, 100, 200);
    MatchResult buyRes = engine->processOrder(buyReq);
    ASSERT_EQ(buyRes.status, OrderStatus::NEW);

    OrderRequest sellReq = createTestLimitRequest(false, 100, 300, 2);
    MatchResult sellRes = engine->processOrder(sellReq);
    ASSERT_EQ(sellRes.status, OrderStatus::NEW);

    ModifyResult modRes = engine->modifyOrder(1, buyRes.orderID, 100, 350);
    ASSERT_EQ(modRes.event.status, statusCodes::ModifyStatus::ACCEPTED);
    ASSERT_NE(modRes.event.newOrderID, modRes.event.oldOrderID);

    ASSERT_EQ(modRes.result.value().tradeVec.size(), 1);

    std::optional<const Order*> modifiedOrder = engine->getOrder(modRes.event.newOrderID);
    ASSERT_FALSE(modifiedOrder.has_value());

    std::optional<const Order*> restingSellOrder = engine->getOrder(sellRes.orderID);
    ASSERT_FALSE(restingSellOrder.has_value());
}

TEST_F(MatchingEngineTest, CancelInvalidClient) {
    OrderRequest req = createTestLimitRequest(true, 100, 200);
    MatchResult res = engine->processOrder(req);
    ASSERT_FALSE(engine->cancelOrder(2, res.orderID));
}

TEST_F(MatchingEngineTest, DoubleCancel) {
    OrderRequest req = createTestLimitRequest(true, 100, 200);
    MatchResult res = engine->processOrder(req);
    ASSERT_TRUE(engine->cancelOrder(1, res.orderID));
    ASSERT_FALSE(engine->cancelOrder(1, res.orderID));
}

TEST_F(MatchingEngineTest, TryToFillCancelled) {
    OrderRequest req = createTestLimitRequest(true, 100, 200);
    MatchResult res = engine->processOrder(req);
    ASSERT_TRUE(engine->cancelOrder(1, res.orderID));

    OrderRequest fillerReq = createTestLimitRequest(false, 100, 200);
    MatchResult fillRes = engine->processOrder(fillerReq);
    ASSERT_EQ(fillRes.tradeVec.size(), 0);
}

// modifying the price or qty to 0 is invalid
TEST_F(MatchingEngineTest, ModifyToZeroQty) {
    OrderRequest req = createTestLimitRequest(true, 100, 200);
    MatchResult res = engine->processOrder(req);
    ModifyResult modRes = engine->modifyOrder(1, res.orderID, 0, 200);

    ASSERT_EQ(modRes.event.status, statusCodes::ModifyStatus::INVALID);
    std::optional<const Order*> order = engine->getOrder(res.orderID);

    ASSERT_TRUE(order.has_value());
    ASSERT_NE(modRes.event.newOrderID, modRes.event.oldOrderID);
    ASSERT_EQ(modRes.event.newOrderID, 0);
}

TEST_F(MatchingEngineTest, ModifyToZeroPrice) {
    OrderRequest req = createTestLimitRequest(true, 100, 200);
    MatchResult res = engine->processOrder(req);
    ModifyResult modRes = engine->modifyOrder(1, res.orderID, 100, 0);

    ASSERT_EQ(modRes.event.status, statusCodes::ModifyStatus::INVALID);
    std::optional<const Order*> order = engine->getOrder(res.orderID);

    ASSERT_TRUE(order.has_value());
    ASSERT_NE(modRes.event.newOrderID, modRes.event.oldOrderID);
    ASSERT_EQ(modRes.event.newOrderID, 0);
}

TEST_F(MatchingEngineTest, ModifyToNegativeQty) {
    OrderRequest req = createTestLimitRequest(true, 100, 200);
    MatchResult res = engine->processOrder(req);
    ModifyResult modRes = engine->modifyOrder(1, res.orderID, -10, 200);

    ASSERT_EQ(modRes.event.status, statusCodes::ModifyStatus::INVALID);
    std::optional<const Order*> order = engine->getOrder(res.orderID);

    ASSERT_TRUE(order.has_value());
    ASSERT_NE(modRes.event.newOrderID, modRes.event.oldOrderID);
    ASSERT_EQ(modRes.event.newOrderID, 0);
}

TEST_F(MatchingEngineTest, ModifyToNegativePrice) {
    OrderRequest req = createTestLimitRequest(true, 100, 200);
    MatchResult res = engine->processOrder(req);
    ModifyResult modRes = engine->modifyOrder(1, res.orderID, 100, -50);

    ASSERT_EQ(modRes.event.status, statusCodes::ModifyStatus::INVALID);
    std::optional<const Order*> order = engine->getOrder(res.orderID);

    ASSERT_TRUE(order.has_value());
    ASSERT_NE(modRes.event.newOrderID, modRes.event.oldOrderID);
    ASSERT_EQ(modRes.event.newOrderID, 0);
}

TEST_F(MatchingEngineTest, ResetEngine) {
    OrderRequest req1 = createTestLimitRequest(true, 100, 200);
    OrderRequest req2 = createTestLimitRequest(false, 100, 300);

    engine->processOrder(req1);
    engine->processOrder(req2);

    ASSERT_EQ(engine->getBidsSize(), 1);
    ASSERT_EQ(engine->getAskSize(), 1);

    engine->reset();

    ASSERT_EQ(engine->getBidsSize(), 0);
    ASSERT_EQ(engine->getAskSize(), 0);
}

TEST_F(MatchingEngineTest, SelfTrading) {
    OrderRequest req1 = createTestLimitRequest(true, 100, 200);
    OrderRequest req2 = createTestLimitRequest(false, 100, 199);

    engine->processOrder(req1);
    MatchResult res = engine->processOrder(req2);

    ASSERT_EQ(res.tradeVec.size(), 0);

    ASSERT_EQ(engine->getAskSize(), 1);
    ASSERT_EQ(engine->getBidsSize(), 1);
}