#include "api/api.hpp"
#include "core/matchingEngine.hpp"
#include "gateway/gateway.hpp"
#include "market-data/MDPublisher.hpp"
#include "market-data/bookEvent.hpp"
#include "market-data/observer.hpp"
#include "protocol/protocolHandler.hpp"
#include "sessions/sessionManager.hpp"
#include "utils/spsc_queue.hpp"
#include "utils/types.hpp"

#include "utils/sharedRegion.hpp"

#include <atomic>
#include <bit>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <ostream>
#include <thread>

MiniExchangeGateway* g_gateway = nullptr;
std::atomic<bool> g_shutdownRequested{false};

void signalHandler(int) {
    g_shutdownRequested.store(true, std::memory_order_relaxed);
    if (g_gateway) {
        g_gateway->stop();
    }
}

int main(int argc, char** argv) {
    try {
        uint16_t port = 12345;
        InstrumentID instrumentID{1};

        if (argc > 1) {
            port = static_cast<std::uint16_t>(std::atoi(argv[1]));
        }

        std::cout << "Starting MiniExchange on port " << port << std::endl;

        std::size_t capacity = 1023;

        std::size_t l2Size = sizeof(utils::spsc_queue_shm<L2OrderBookUpdate>) +
                             sizeof(L2OrderBookUpdate) * std::bit_ceil(capacity + 1);

        std::size_t l3Size = sizeof(utils::spsc_queue_shm<L3Update>) +
                             sizeof(L3Update) * std::bit_ceil(capacity + 1);

        SharedRegion l2Region(l2Size, "/l2queue");
        SharedRegion l3Region(l3Size, "/l3queue");

        auto* l2Queue =
            new (l2Region.data()) utils::spsc_queue_shm<L2OrderBookUpdate>(capacity);
        auto* l3Queue = new (l3Region.data()) utils::spsc_queue_shm<L3Update>(capacity);

        auto mdQueue = std::make_unique<utils::spsc_queue<L2OrderBookUpdate>>(1024);

        MatchingEngine engine(l2Queue, l3Queue, instrumentID);
        std::cout << "Matching engine initialized" << std::endl;

        Level2OrderBook level2Book;
        Level3OrderBook level3book;

        market_data::Observer observer(l2Queue, mdQueue.get(), level2Book, level3book,
                                       instrumentID);
        std::cout << "Observer initialized" << std::endl;

        market_data::UDPConfig mdConfig{};
        market_data::PublisherConfig pubCfg{};
        market_data::MarketDataPublisher mdPublisher(mdQueue.get(), level2Book,
                                                     instrumentID, pubCfg);

        std::cout << "Market data publisher initialized" << std::endl;

        std::jthread observerThread([&]() {
            while (!g_shutdownRequested.load(std::memory_order_relaxed)) {
                observer.drainQueue();
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
            std::cout << "Observer thread shutting down" << std::endl;
        });

        std::jthread mdPublisherThread([&]() {
            while (!g_shutdownRequested.load(std::memory_order_relaxed)) {
                mdPublisher.runOnce();
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }

            std::cout << "Market data publisher thread shutting down" << std::endl;
        });

        SessionManager sessions;
        std::cout << "Session manager initialized" << std::endl;

        MiniExchangeAPI api(engine, sessions);
        std::cout << "Exchange API initialized" << std::endl;

        ProtocolHandler handler(sessions, api);
        std::cout << "Protocol handler initialized" << std::endl;

        MiniExchangeGateway gateway(handler, sessions, port);
        g_gateway = &gateway;
        std::cout << "Network gateway initialized" << std::endl;

        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);
        std::cout << "Signal handlers installed" << std::endl;

        std::cout << "\nExchange ready - waiting for connections..." << std::endl;
        std::cout << "Press Ctrl+C to shutdown gracefully\n" << std::endl;

        while (!g_shutdownRequested.load(std::memory_order_relaxed)) {
            gateway.run();
        }

        std::cout << "\nExchange shutdown complete" << std::endl << std::flush;

        g_gateway = nullptr;

        return EXIT_SUCCESS;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
