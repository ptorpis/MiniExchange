#include "client/networkClient.hpp"
#include "utils/types.hpp"
#include <chrono>
#include <iostream>
#include <limits>
#include <thread>

class TestClient : public NetworkClient {
public:
    using NetworkClient::NetworkClient;

protected:
    void onHelloAck(const Message<server::HelloAckPayload>& msg) override {
        std::cout << "Hello ACK received" << std::endl;
        utils::printMessage(std::cout, msg);
    }

    void onOrderAck(const Message<server::OrderAckPayload>& msg) override {
        std::cout << "Order ACK - OrderID: " << msg.payload.serverOrderID
                  << ", Status: " << static_cast<int>(msg.payload.status) << std::endl;
        utils::printMessage(std::cout, msg);
    }

    void onTrade(const Message<server::TradePayload>& msg) override {
        std::cout << "Trade - Qty: " << msg.payload.filledQty
                  << " @ Price: " << msg.payload.filledPrice << std::endl;
        utils::printMessage(std::cout, msg);
    }
};

void waitForEnter(const std::string& message) {
    std::cout << "\n" << message << " [Press Enter]" << std::flush;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

int main() {
    TestClient client("127.0.0.1", 12345);

    waitForEnter("Press Enter to connect...");

    if (!client.connect()) {
        std::cerr << "Failed to connect" << std::endl;
        return 1;
    }
    std::cout << "Connected to exchange" << std::endl;

    waitForEnter("Press Enter to send HELLO...");
    client.sendHello();

    std::cout << "Processing responses..." << std::endl;
    for (int i = 0; i < 10; ++i) {
        client.processMessages();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    waitForEnter("Press Enter to send NEW ORDER...");

    client::NewOrderPayload order{};
    order.serverClientID = 1;
    order.instrumentID = 1;
    order.orderSide = +(OrderSide::BUY);
    order.orderType = +(OrderType::LIMIT);
    order.timeInForce = 0;
    order.qty = 100;
    order.price = 15000;
    order.goodTillDate = Timestamp{0};

    client.sendNewOrder(order);
    std::cout << "Sent order" << std::endl;

    std::cout << "Processing responses..." << std::endl;
    for (int i = 0; i < 100; ++i) {
        client.processMessages();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    waitForEnter("Press Enter to disconnect...");

    client.disconnect();
    std::cout << "Disconnected" << std::endl;

    return 0;
}
