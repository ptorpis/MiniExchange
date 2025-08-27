#pragma once

#include <cstdint>
#include <iomanip>
#include <iostream>

namespace utils {
void printHex(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if ((i % 16) == 0) {
            std::cout << std::endl;
        }
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(data[i]) << " ";
    }
    std::cout << "\n\n";
}

} // namespace utils