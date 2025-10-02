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
        try {
            net.receiveMessage();
        } catch (const std::exception& e) {
            std::cerr << "Receive error: " << e.what() << '\n';
            client.stop();
            return;
        }
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
    // std::jthread hbThread(heartbeatLoop, std::ref(net), std::ref(c), 2);
    std::jthread recThread(receiveLoop, std::ref(net), std::ref(c));

    std::string line;
    while (c.isRunning()) {
        std::cout << "> ";
        if (!std::getline(std::cin, line) || !c.isRunning()) break;

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "hello") {
            c.sendHello();
            net.sendMessage();
        } else if (cmd == "order") {
            Price price;
            Qty qty;
            std::string sideStr, typeStr;

            bool isBuy, isLimit;

            if (!(iss >> sideStr >> qty >> typeStr >> price)) {
                std::cout << "Usage: order [buy | sell] <qty> [limit | market] <price>"
                          << std::endl;
                continue;
            }

            if (sideStr == "buy") {
                isBuy = true;
            } else if (sideStr == "sell") {
                isBuy = false;
            } else {
                std::cout << "Invalid side. (must be 'buy' or 'sell')" << std::endl;
                continue;
            }

            if (typeStr == "limit") {
                isLimit = true;
            } else if (sideStr == "market") {
                isLimit = false;
            } else {
                std::cout << "Invalid type. (must be 'limit' or 'market')" << std::endl;
                continue;
            }

            c.sendOrder(qty, price, isBuy, isLimit);
            net.sendMessage();

            std::cout << "Order Submitted: qty=" << qty << " price=" << price
                      << " side=" << (isBuy ? "BUY" : "SELL")
                      << " type=" << (isLimit ? "LIMIT" : "MARKET") << std::endl;

        } else if (line == "stop") {
            // hbThread.request_stop();
            recThread.request_stop();

            std::cout << "Heartbeat stopped" << std::endl;
            std::cout << "Exiting..." << std::endl;

            // hbThread.join();
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