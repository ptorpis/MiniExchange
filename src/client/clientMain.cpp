#include "client/tradingClient.hpp"
#include <atomic>
#include <csignal>
#include <iostream>
#include <random>
#include <thread>

std::atomic<bool> g_running{true};

void signalHandler(int) {
    g_running.store(false, std::memory_order_relaxed);
}

class SimpleStrategy : public TradingClient {
public:
    using TradingClient::TradingClient;

protected:
    void onOrderSubmitted(ClientOrderID clientOrderID) override {
        std::cout << "[ORDER] Submitted " << clientOrderID.value() << std::endl;
    }

    void onOrderAccepted(ClientOrderID clientOrderID, OrderID serverOrderID,
                         Price acceptedPrice) override {
        std::cout << "[ORDER] Accepted " << clientOrderID.value()
                  << " (server: " << serverOrderID.value() << ") @ "
                  << acceptedPrice.value() << std::endl;
    }

    void onOrderFilled(ClientOrderID clientOrderID, Price fillPrice,
                       Qty fillQty) override {
        std::cout << "[FILL] " << fillQty.value() << " @ " << fillPrice.value()
                  << " (order " << clientOrderID.value() << ")" << std::endl;
    }

    void onOrderRejected(ClientOrderID clientOrderID,
                         [[maybe_unused]] OrderStatus status) override {
        std::cout << "[ORDER] Rejected " << clientOrderID.value() << std::endl;
    }

    void onBookSnapshot(const Level2OrderBook& book, std::uint64_t seqNum) override {
        std::cout << "[MD] Snapshot " << seqNum << ": " << book.bids.size() << " bids, "
                  << book.asks.size() << " asks" << std::endl;
    }

    void onBookValid() override { std::cout << "[MD] Book valid" << std::endl; }

    void onBookDelta([[maybe_unused]] Price price, [[maybe_unused]] Qty qty,
                     [[maybe_unused]] OrderSide side, [[maybe_unused]] MDDeltatype type,
                     [[maybe_unused]] std::uint64_t seqNum) override {
        std::cout << "Delta received: " << side << std::endl;
    }

    void onBookInvalid() override { std::cout << "[MD] Book invalid" << std::endl; }

    void onGapDetected(std::uint64_t expected, std::uint64_t received) override {
        std::cout << "[MD] Gap: expected " << expected << ", got " << received
                  << std::endl;
    }
};

int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Configuration
    TradingConfig config;
    config.host = "127.0.0.1";
    config.port = 12345;
    config.mdConfig.multicastGroup = "239.0.0.1";
    config.mdConfig.port = 9001;
    config.enabledMarketData = true;

    std::cout << "Trading Client" << std::endl;
    std::cout << "Trading: " << config.host << ":" << config.port << std::endl;
    std::cout << "Market Data: " << config.mdConfig.multicastGroup << ":"
              << config.mdConfig.port << std::endl;
    std::cout << "Press Ctrl+C to exit\n" << std::endl;

    SimpleStrategy strategy(config);

    if (!strategy.connect()) {
        std::cerr << "Failed to connect" << std::endl;
        return 1;
    }

    std::cout << "Connected!\n" << std::endl;

    // Random number generator for orders
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> sideDist(0, 1);
    std::uniform_int_distribution<uint64_t> priceDist(980, 1000);
    std::uniform_int_distribution<uint64_t> qtyDist(1'500, 1'000'000);

    auto lastOrderTime = std::chrono::steady_clock::now();

    while (g_running.load(std::memory_order_relaxed)) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(now - lastOrderTime);

        if (elapsed.count() >= 1) {
            OrderSide side = sideDist(gen) == 0 ? OrderSide::BUY : OrderSide::SELL;
            Price price{priceDist(gen)};
            Qty qty{qtyDist(gen)};

            std::cout << "\n[RANDOM] Sending "
                      << (side == OrderSide::BUY ? "BUY" : "SELL") << " " << qty.value()
                      << " @ " << price.value() << std::endl;

            strategy.submitOrder(InstrumentID{1}, side, qty, price);
            lastOrderTime = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nShutting down..." << std::endl;

    auto pos = strategy.getPosition(InstrumentID{1});
    std::cout << "Final position: " << pos.netPosition() << std::endl;

    strategy.disconnect();
    return 0;
}
