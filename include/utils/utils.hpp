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
inline void printHex(std::span<const uint8_t> data) {
    for (size_t i = 0; i < data.size(); i++) {
        if ((i % 16) == 0) {
            std::cout << "\n";
        }
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(data[i]) << " ";
    }
    std::cout << "\n\n";
}

inline void printHex(const void* data, size_t size) {
    auto ptr = static_cast<const uint8_t*>(data);
    printHex(std::span<const uint8_t>(ptr, size));
}

// Convenience overload: takes any container with contiguous storage
template <typename Container> inline void printHex(const Container& c) {
    printHex(
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(c.data()), c.size()));
}

inline uint64_t getCurrentTimestampMicros() {
    auto now = std::chrono::steady_clock::now();
    auto epoch = now.time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(epoch).count());
}

} // namespace utils