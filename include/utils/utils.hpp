#pragma once

#include "types.hpp"
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <span>
#include <type_traits>

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
    std::cout << std::dec << "\n\n";
}

inline void printHex(const void* data, std::size_t size) {
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

inline Timestamp getTimestampNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// evaluates to TRUE if the number was incremented by exactly one, otherwise returns false
inline bool isCorrectIncrement(const uint32_t curr, const uint32_t next) {
    return curr - next == 1;
}

} // namespace utils
