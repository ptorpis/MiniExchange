#include "server/server.hpp"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

int main(int argc, char* argv[]) {
    std::string address;
    uint16_t port;
    int n_events = 64;

    try {
        std::cout << "Loading config.json\n";
        std::ifstream config_file("config/config.json");
        json config;
        config_file >> config;

        address = config.value("address", "127.0.0.1");
        port = config.value("port", 12345);
        n_events = config.value("max_events", 64);

    } catch (...) {
        std::cerr << "Failed to load config.json, using defaults\n";
        address = "127.0.0.1";
        port = 12345;
        n_events = 64;
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--address" && i + 1 < argc) {
            address = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--events" && i + 1 < argc) {
            n_events = std::stoi(argv[++i]);
        }
    }

    Server server(address, port, n_events);
    server.start();
    server.stop();
    return 0;
}