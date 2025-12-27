#include "client/tradingClient.hpp"
#include "utils/types.hpp"
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <print>
#include <string>
#include <thread>

class SimpleStrategy : public TradingClient {
public:
    SimpleStrategy(std::string host, std::uint16_t port)
        : TradingClient(std::move(host), port) {}

protected:
    void onOrderSubmitted(ClientOrderID clientOrderID) override {
        std::cout << "Order " << clientOrderID << " submitted\n";
    }

    void onOrderAccepted(ClientOrderID clientOrderID, OrderID serverOrderID,
                         Price acceptedPrice) override {
        std::cout << "Order " << clientOrderID
                  << " accepted (server ID: " << serverOrderID << ") @ " << acceptedPrice
                  << "\n";
    }

    void onOrderRejected(ClientOrderID clientOrderID, OrderStatus reason) override {
        std::cout << "Order " << clientOrderID.value()
                  << " rejected: " << static_cast<int>(reason) << std::endl;
    }

    void onOrderFilled(ClientOrderID clientOrderID, Price fillPrice,
                       Qty fillQty) override {
        std::cout << "Filled " << fillQty.value() << " @ " << fillPrice.value()
                  << " (order " << clientOrderID.value() << ")" << std::endl;

        auto order = getOrder(clientOrderID);
        if (order) {
            auto pos = getPosition(order->instrumentID);
            std::cout << "  Position: " << pos.netPosition() << std::endl;
        }
    }
};

int main() {
    SimpleStrategy client{"127.0.0.1", 12345};
    if (!client.connect()) {
        std::cerr << "Failed to connect\n";
        return EXIT_FAILURE;
    }

    client.sendHello();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    client.submitOrder(InstrumentID{1}, OrderSide::BUY, Qty{100}, Price{15000},
                       OrderType::LIMIT, TimeInForce::GOOD_TILL_CANCELLED, Timestamp{0});

    client.submitOrder(InstrumentID{1}, OrderSide::SELL, Qty{100}, Price{15000},
                       OrderType::LIMIT, TimeInForce::GOOD_TILL_CANCELLED, Timestamp{0});

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto open = client.getOpenOrders();
    std::println("Open orders {}", open.size());

    auto pos = client.getPosition(InstrumentID{1});

    std::println("Position {}", pos.netPosition());
    client.disconnect();

    return 0;
}
