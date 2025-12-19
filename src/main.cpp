#include "core/order.hpp"
#include "utils/timing.hpp"
#include "utils/types.hpp"
#include <print>

int main() {
    Order order{.orderID = OrderID{1},
                .clientID = ClientID{2},
                .qty = Qty{3},
                .price = Price{100},
                .goodTill = 100,
                .timestamp = TSCClock::TSCClock::now(),
                .instrumentID = 1,
                .tif = TimeInForce::GOOD_TILL_CANCELLED,
                .side = OrderSide::BUY,
                .type = OrderType::LIMIT,
                .status = OrderStatus::NEW};

    std::cout << order;

    std::println("Size: {}", sizeof(order));

    Price price1{100};
    Price price2{150};
    Qty qty1{200};
    Qty qty2{300};

    std::cout << price1 + price2 << std::endl;

    std::cout << qty1 + qty2 << std::endl;
}
