#include "core/order.hpp"
#include "utils/timing.hpp"
#include <print>

int main() {
    Order order{.orderID = 1,
                .clientID = 2,
                .qty = 3,
                .price = 100,
                .goodTill = 100,
                .timestamp = TSCClock::TSCClock::now(),
                .instrumentID = 1,
                .tif = TimeInForce::GOOD_TILL_CANCELLED,
                .side = OrderSide::BUY,
                .type = OrderType::LIMIT,
                .status = OrderStatus::NEW};

    std::cout << order;

    std::println("Size: {}", sizeof(order));
}
