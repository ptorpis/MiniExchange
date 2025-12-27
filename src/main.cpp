#include "api/api.hpp"
#include "core/matchingEngine.hpp"
#include "gateway/gateway.hpp"
#include "protocol/protocolHandler.hpp"
#include "sessions/sessionManager.hpp"
#include "utils/types.hpp"

#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>

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

        MatchingEngine engine(instrumentID);
        std::cout << "Matching engine initialized" << std::endl;

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

        std::cout << "\nExchange shutdown complete" << std::endl;
        g_gateway = nullptr;

        return EXIT_SUCCESS;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
