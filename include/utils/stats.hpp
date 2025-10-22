#pragma once

#include <atomic>
#include <cstdint>
#include <iomanip>
#include <iostream>

namespace utils {
struct ClientStats {
    std::atomic<uint64_t> newOrders{0};
    std::atomic<uint64_t> cancels{0};
    std::atomic<uint64_t> modifies{0};
    std::atomic<uint64_t> heartbeats{0};

    std::atomic<uint64_t> newOrdersThisSecond{0};
    std::atomic<uint64_t> cancelsThisSecond{0};
    std::atomic<uint64_t> modifiesThisSecond{0};

    uint64_t getTotalActions() const {
        return newOrders.load() + cancels.load() + modifies.load();
    }

    uint64_t getActionsThisSecond() const {
        return newOrdersThisSecond.load() + cancelsThisSecond.load() +
               modifiesThisSecond.load();
    }

    void resetPerSecondCounters() {
        newOrdersThisSecond.store(0);
        cancelsThisSecond.store(0);
        modifiesThisSecond.store(0);
    }

    void printSummary() const {
        std::cout << "\n+------------------------------------+\n";
        std::cout << "|     Simulation Summary             |\n";
        std::cout << "+------------------------------------+\n";
        std::cout << "| Total Actions: " << std::setw(19) << getTotalActions() << " |\n";
        std::cout << "|    New Orders: " << std::setw(19) << newOrders.load() << " |\n";
        std::cout << "|       Cancels: " << std::setw(19) << cancels.load() << " |\n";
        std::cout << "|      Modifies: " << std::setw(19) << modifies.load() << " |\n";
        std::cout << "|    Heartbeats: " << std::setw(19) << heartbeats.load() << " |\n";
        std::cout << "+------------------------------------+\n";
    }
};

}; // namespace utils