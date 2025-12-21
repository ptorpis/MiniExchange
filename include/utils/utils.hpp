#pragma once

#include <concepts>
#include <type_traits>

template <typename T> constexpr auto operator+(T a) noexcept {
    return static_cast<std::underlying_type_t<T>>(a);
}

namespace utils {

template <std::unsigned_integral T> bool isCorrectIncrement(const T prev, const T curr) {
    return (curr - prev) == 1;
}
} // namespace utils
