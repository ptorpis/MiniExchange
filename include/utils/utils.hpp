#pragma once

#include "protocol/messages.hpp"
#include <concepts>
#include <iomanip>
#include <iostream>
#include <type_traits>

#include <concepts>
#include <ostream>
#include <type_traits>

// Concept for types that provide iterateElementsWithNames
template <typename T>
concept Reflectable = requires(T t) {
    // Must be callable with some generic lambda
    t.iterateElementsWithNames([](auto&&, auto&&) {});
};

template <typename T> constexpr auto operator+(T a) noexcept {
    return static_cast<std::underlying_type_t<T>>(a);
}
namespace utils {

template <Reflectable Payload>
void printMessage(std::ostream& os, const Message<Payload>& msg) {
    auto printField = [&](const char* name, const auto& field) {
        using FieldT = std::remove_reference_t<decltype(field)>;
        if constexpr (!std::is_array_v<FieldT>) {
            os << "\t" << name << " = " << +field << " (0x" << std::hex << +field
               << std::dec << ")\n";
        }
    };

    os << "\n\033[32m###[MESSAGE]###\033[0m\n";

    os << "[HEADER]\n";
    msg.header.iterateElementsWithNames(printField);

    os << "[PAYLOAD]\n";
    msg.payload.iterateElementsWithNames(printField);

    os << std::flush;
}

template <typename T> void printScalar(std::ostream& os, const T& value) {
    if constexpr (std::is_enum_v<T>) {
        using U = std::underlying_type_t<T>;
        U v = static_cast<U>(value);
        os << +v << "(0x" << std::hex << +v << std::dec << ")";
    } else {
        os << +value << "(0x" << std::hex << +value << std::dec << ")";
    }
}

template <typename T> void printArray(std::ostream& os, const T& arr) {
    os << "[";
    for (std::size_t i = 0; i < std::extent_v<T>; ++i) {
        if (i != 0) os << " ";
        os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(arr[i]);
    }
    os << std::dec << "]";
}

template <typename T> void printReflected(std::ostream& os, const T& obj) {
    bool first = true;

    obj.iterateElementsWithNames([&](const char* name, const auto& field) {
        using FieldT = std::remove_reference_t<decltype(field)>;

        if (!first) os << " | ";
        first = false;

        os << name << "=";

        if constexpr (std::is_array_v<FieldT>) {
            printArray(os, field);
        } else {
            printScalar(os, field);
        }
    });
}

template <std::unsigned_integral T> bool isCorrectIncrement(const T prev, const T curr) {
    return (curr - prev) == 1;
}

inline void printHex(std::span<const std::byte> data) {
    for (auto i{0uz}; i < data.size(); i++) {
        if ((i % 16) == 0) {
            std::cout << "\n";
        }
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(data[i]) << " ";
    }
    std::cout << std::dec << "\n\n";
}
} // namespace utils
