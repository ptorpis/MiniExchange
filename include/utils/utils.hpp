#pragma once

#include "utils/types.hpp"
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <utility>

template <typename T> constexpr auto operator+(T a) noexcept {
    return static_cast<std::underlying_type_t<T>>(a);
}

namespace utils {
void inline printHex(std::span<const uint8_t> data) {
    for (size_t i = 0; i < data.size(); i++) {
        if ((i % 16) == 0) {
            std::cout << std::endl;
        }
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(data[i]) << " ";
    }
    std::cout << "\n\n";
}

inline uint64_t getCurrentTimestampMicros() {
    auto now = std::chrono::steady_clock::now();
    auto epoch = now.time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(epoch).count());
}

} // namespace utils