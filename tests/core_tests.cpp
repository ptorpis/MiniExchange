#include "core/matchingEngine.hpp"
#include <gtest/gtest.h>

class MatchingEngineTest : public ::testing::Test {
protected:
    void SetUp() override { engine = std::make_unique<MatchingEngine>(); }

    std::unique_ptr<MatchingEngine> engine;
};

TEST_F(MatchingEngineTest, EmptyBookHasNoSpread) {
    EXPECT_FALSE(engine->getBestAsk().has_value());
    EXPECT_FALSE(engine->getBestBid().has_value());
    EXPECT_FALSE(engine->getSpread().has_value());
}