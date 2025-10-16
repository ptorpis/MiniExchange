#include "client/client.hpp"
#include "events/eventBus.hpp"
#include "logger/eventLogger.hpp"
#include "protocol/client/clientMessageFactory.hpp"
#include "protocol/messages.hpp"
#include "protocol/protocolHandler.hpp"
#include "protocol/traits.hpp"
#include "server/server.hpp"
#include "utils/timing.hpp"

#include <iostream>
#include <memory>

int main() {
    TSCClock::calibrate();
    utils::startTimingConsumer("timings.csv");
    std::cout << "Timer calibrated: " << TSCClock::nsPerTick << "ns/Tick" << std::endl;
    auto evBus = std::make_shared<EventBus>();
    evBus->start();
    auto tscStart = TSCClock::now();
    auto clockStart = std::chrono::steady_clock::now();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto tscEnd = TSCClock::now();
    auto clockEnd = std::chrono::steady_clock::now();

    uint64_t elapsedTscNs = uint64_t((tscEnd - tscStart) * TSCClock::nsPerTick);
    auto elapsedClockNs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(clockEnd - clockStart)
            .count();

    std::cout << "TSC elapsed ns: " << elapsedTscNs << "\n";
    std::cout << "SteadyClock elapsed ns: " << elapsedClockNs << "\n";

    auto recvLogger = std::make_shared<GenericEventLogger<ReceiveMessageEvent>>(
        evBus, "recv_messages.csv");
    auto addedLogger = std::make_shared<GenericEventLogger<AddedToBookEvent>>(
        evBus, "added_to_book.csv");
    auto cancelLogger = std::make_shared<GenericEventLogger<OrderCancelledEvent>>(
        evBus, "cancelled_orders.csv");
    auto modifyLogger =
        std::make_shared<GenericEventLogger<ModifyEvent>>(evBus, "modified_orders.csv");
    auto tradeLogger =
        std::make_shared<GenericEventLogger<TradeEvent>>(evBus, "trades.csv");
    auto removeLogger = std::make_shared<GenericEventLogger<RemoveFromBookEvent>>(
        evBus, "removed_from_book.csv");

    SessionManager sessionManager;
    ProtocolHandler handler(sessionManager, evBus);
    uint16_t port = 12345;

    Server server(port, sessionManager, handler, evBus);
    if (!server.start(port)) {
        return 1;
    }
    server.run();

    evBus->stop();
    return 0;
}