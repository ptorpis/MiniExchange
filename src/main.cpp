#include <iostream>

#include "core/matchingEngine.hpp"
#include "core/order.hpp"
#include "utils/types.hpp"

int main() {

    MatchingEngine engine;
    Qty qty(101);
    Price price(200);

    Qty qty2(100);
    Price price2(200);

    Order order{1,
                1,
                OrderSide::BUY,
                OrderType::LIMIT,
                qty,
                price,
                TimeInForce::GTC,
                0,
                OrderStatus::NEW,
                1};

    Order order2{2,
                 2,
                 OrderSide::SELL,
                 OrderType::LIMIT,
                 qty2,
                 price2,
                 TimeInForce::GTC,
                 0,
                 OrderStatus::NEW,
                 1};

    engine.processOrder(&order);
    engine.processOrder(&order2);

    return 0;
}