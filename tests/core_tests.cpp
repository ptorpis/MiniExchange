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
    Qty qty = 100;
    Price price = 200;
    OrderRequest mktreq = createTestMarketRequest(true, qty, price);

    MatchResult result = engine->processOrder(mktreq);

    EXPECT_EQ(result.tradeVec.size(), 0);
}