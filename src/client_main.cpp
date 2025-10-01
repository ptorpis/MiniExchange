#include "client/client.hpp"
#include "client/clientNetwork.hpp"
#include "utils/types.hpp"
#include <chrono>
#include <string>
#include <thread>

void heartbeatLoop(std::stop_token stoken, ClientNetwork& net, Client& client,
                   int intervalSeconds = 2) {
    while (!stoken.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::seconds(intervalSeconds));
        client.sendHeartbeat();
        net.sendMessage();
    }
}

void receiveLoop(std::stop_token stoken, ClientNetwork& net, Client& client) {
    while (!stoken.stop_requested()) {
        net.receiveMessage();
        client.processIncoming();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

int main() {
    ApiKey apiKey;
    HMACKey hmacKey;
    std::fill(apiKey.begin(), apiKey.end(), 0x22);
    std::fill(hmacKey.begin(), hmacKey.end(), 0x11);

    Client c(hmacKey, apiKey);
    ClientNetwork net("127.0.0.1", 12345, c);

    if (!net.connectServer()) return -1;
    std::jthread hbThread(heartbeatLoop, std::ref(net), std::ref(c), 2);
    std::jthread recThread(receiveLoop, std::ref(net), std::ref(c));

    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;

        if (line == "hello") {
            c.sendHello();
            net.sendMessage();
        } else if (line == "stop") {
            hbThread.request_stop();
            recThread.request_stop();

            std::cout << "Heartbeat stopped" << std::endl;
            std::cout << "Exiting..." << std::endl;

            hbThread.join();
            recThread.join();
            break;
        } else {
            std::cout << "Unknown Message" << std::endl;
            continue;
        }
    }

    net.disconnectServer();

    return 0;
}