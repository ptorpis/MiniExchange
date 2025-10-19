#include <atomic>
#include <csignal>
#include <iostream>
#include <thread>

#include "client/clientRunner.hpp"

std::atomic<bool> gRunning{true};

void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    gRunning = false;
}

int main() {
    std::signal(SIGINT, signalHandler);

    std::string serverIP = "127.0.0.1";
    uint16_t port = 12345;
    size_t nClients = 200;

    ClientRunner runner(serverIP, port, nClients);

    std::thread runnerThread([&runner]() { runner.start(); });

    while (gRunning) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    runner.stop();
    runnerThread.join();

    std::cout << "ClientRunner stopped cleanly.\n";
    return 0;
}
