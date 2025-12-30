#include "client/tradingClient.hpp"
#include <iostream>
#include <thread>

class SimpleStrategy : public TradingClient {
public:
    using TradingClient::TradingClient;

protected:
    void onOrderSubmitted(ClientOrderID clientOrderID) override {
        std::cout << " Order " << clientOrderID.value() << " submitted" << std::endl;
    }

    void onOrderAccepted(ClientOrderID clientOrderID, OrderID serverOrderID,
                         Price acceptedPrice) override {
        std::cout << " Order " << clientOrderID.value()
                  << " accepted (server: " << serverOrderID.value() << ") @ "
                  << acceptedPrice.value() << std::endl;
    }

    void onOrderFilled(ClientOrderID clientOrderID, Price fillPrice,
                       Qty fillQty) override {
        std::cout << " Filled " << fillQty.value() << " @ " << fillPrice.value()
                  << " (order " << clientOrderID.value() << ")" << std::endl;

        auto order = getOrder(clientOrderID);
        if (order) {
            auto pos = getPosition(order->instrumentID);
            std::cout << "  Position: " << pos.netPosition() << std::endl;
        }
    }
};

int main() {
    SimpleStrategy strategy("127.1.1.0", 12345);

    if (!strategy.connect()) {
        std::cerr << "Failed to connect" << std::endl;
        return 1;
    }

    std::cout << "Connected!" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    strategy.submitOrder(InstrumentID{1}, OrderSide::BUY, Qty{100}, Price{15000});
    strategy.submitOrder(InstrumentID{1}, OrderSide::SELL, Qty{100}, Price{15000});

    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto pos = strategy.getPosition(InstrumentID{1});
    std::cout << "\nFinal position: " << pos.netPosition() << std::endl;

    strategy.disconnect();
    return 0;
}
