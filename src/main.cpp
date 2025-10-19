#include "client/client.hpp"
#include "events/eventBus.hpp"
#include "logger/addLogger.hpp"
#include "protocol/client/clientMessageFactory.hpp"
#include "protocol/messages.hpp"
#include "protocol/protocolHandler.hpp"
#include "protocol/traits.hpp"
#include "server/server.hpp"
#include "utils/timing.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

nlohmann::json readRunConfig(const std::filesystem::path& cfgPath) {
    std::ifstream f(cfgPath);
    if (!f.is_open()) {
        throw std::runtime_error("Could not open run config: " + cfgPath.string());
    }
    nlohmann::json j;
    f >> j;
    return j;
}

void enrichRunConfig(nlohmann::json& cfg, const std::filesystem::path& outputDir) {
    cfg["baseTick"] = TSCClock::now();
    cfg["nsPerTick"] = TSCClock::nsPerTick;
    nlohmann::json files;

#ifdef ENABLE_LOGGING
    files.update({{"recv_messages", (outputDir / "recv_messages.csv").string()},
                  {"added_to_book", (outputDir / "added_to_book.csv").string()},
                  {"cancelled_orders", (outputDir / "cancelled_orders.csv").string()},
                  {"modified_orders", (outputDir / "modified_orders.csv").string()},
                  {"trades", (outputDir / "trades.csv").string()},
                  {"removed_from_book", (outputDir / "removed_from_book.csv").string()},
                  {"new_connections", (outputDir / "new_connections.csv").string()},
                  {"disconnects", (outputDir / "disconnects.csv").string()},
                  {"sent_messages", (outputDir / "sent_messages.csv").string()}});
#endif

#ifdef ENABLE_TIMING
    files["timings"] = (outputDir / "timings.csv").string();
#endif

    cfg["files"] = files;
}

void writeRunConfig(const nlohmann::json& cfg, const std::filesystem::path& runDir) {
    std::filesystem::create_directories(runDir);
    std::ofstream out(runDir / "run_config.json");
    out << cfg.dump(4);
}

int main() {
    std::filesystem::path configPath = "config/server_run_cfg.json";
    nlohmann::json runCfg = readRunConfig(configPath);

    std::filesystem::path runDir =
        std::filesystem::path(runCfg["output_base"].get<std::string>()) /
        runCfg["name"].get<std::string>();
    std::filesystem::create_directories(runDir);
    std::cout << "Output directory: " << runDir << std::endl;

#ifdef ENABLE_TIMING
    std::filesystem::path timingsFile = runDir / "timings.csv";
    TSCClock::calibrate();
    utils::startTimingConsumer(timingsFile.c_str());
    std::cout << "Timer calibrated: " << TSCClock::nsPerTick << " ns/Tick" << std::endl;
#endif

    enrichRunConfig(runCfg, runDir);
    writeRunConfig(runCfg, runDir);
    std::cout << "Run config written to " << (runDir / "run_config.json") << std::endl;

    auto evBus = std::make_shared<EventBus>();
    evBus->start();

#ifdef ENABLE_LOGGING
    auto loggers = createLoggers(evBus, runDir);
#endif

    SessionManager sessionManager;
    ProtocolHandler handler(sessionManager, evBus);
    uint16_t port = runCfg["port"].get<uint16_t>();

    Server server(port, sessionManager, handler, evBus);
    if (!server.start(port)) return 1;
    server.run();

    evBus->stop();
    return 0;
}
