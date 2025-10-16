#include "client/client.hpp"
#include "events/eventBus.hpp"
#include "logger/eventLogger.hpp"
#include "protocol/client/clientMessageFactory.hpp"
#include "protocol/messages.hpp"
#include "protocol/protocolHandler.hpp"
#include "protocol/traits.hpp"
#include "server/server.hpp"
#include "utils/timing.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

std::filesystem::path getOutputDir() {
    if (const char* env = std::getenv("EXCHANGE_OUTPUT_DIR")) {
        return env;
    }
    return "output/default";
}

int main() {
    std::filesystem::path outputDir = getOutputDir();
    std::filesystem::create_directories(outputDir);
    std::cout << "Writing to " << outputDir << std::endl;

    TSCClock::calibrate();
    std::string timingsFile = (outputDir / "timings.csv").string();
    utils::startTimingConsumer(timingsFile.c_str());
    std::cout << "Timer calibrated: " << TSCClock::nsPerTick << "ns/Tick" << std::endl;

    auto evBus = std::make_shared<EventBus>();
    evBus->start();

#ifdef ENABLE_LOGGING
    std::string recvFile = (outputDir / "recv_messages.csv").string();
    std::string addedFile = (outputDir / "added_to_book.csv").string();
    std::string cancelFile = (outputDir / "cancelled_orders.csv").string();
    std::string modifyFile = (outputDir / "modified_orders.csv").string();
    std::string tradeFile = (outputDir / "trades.csv").string();
    std::string removeFile = (outputDir / "removed_from_book.csv").string();

    auto recvLogger =
        std::make_shared<GenericEventLogger<ReceiveMessageEvent>>(evBus, recvFile);
    auto addedLogger =
        std::make_shared<GenericEventLogger<AddedToBookEvent>>(evBus, addedFile);
    auto cancelLogger =
        std::make_shared<GenericEventLogger<OrderCancelledEvent>>(evBus, cancelFile);
    auto modifyLogger =
        std::make_shared<GenericEventLogger<ModifyEvent>>(evBus, modifyFile);
    auto tradeLogger = std::make_shared<GenericEventLogger<TradeEvent>>(evBus, tradeFile);
    auto removeLogger =
        std::make_shared<GenericEventLogger<RemoveFromBookEvent>>(evBus, removeFile);
#endif

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
