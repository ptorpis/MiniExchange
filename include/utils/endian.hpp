#pragma once

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>

// Generic version of ntoh functions that work with any unsigned integral type

template <std::unsigned_integral T> inline T swapEndian(T value) {
    if constexpr (std::endian::native == std::endian::little) {
        return std::byteswap(value);
    }

    return value;
}

template <typename T> void swapFieldEndian(T& field) {
    using FieldType = std::decay_t<T>;

    if constexpr (requires { field.data_m; }) {
        using UnderlyingType = decltype(field.data_m);
        if constexpr (std::is_integral_v<UnderlyingType> && sizeof(UnderlyingType) > 1) {
            field.data_m = swapEndian(field.data_m);
        }
    } else if constexpr (std::is_integral_v<FieldType> && sizeof(FieldType) > 1) {
        field = swapEndian(field);
    }
    // else: no-op for single bytes, arrays, etc.
}

inline std::uint8_t readByteAdvance(std::span<const std::byte>& view) {
    std::byte value = view.front();
    view = view.subspan(1);
    return std::to_integer<std::uint8_t>(value);
}

inline void writeByteAdvance(std::byte*& ptr, std::byte val) {
    *ptr = val;
    ++ptr;
}

template <std::unsigned_integral T> void writeIntegerAdvance(std::byte*& ptr, T val) {
    T beVal = swapEndian(val);
    std::memcpy(ptr, &beVal, sizeof(T));
    ptr += sizeof(T);
}

inline void writeBytesAdvance(std::byte*& ptr, const std::byte* src, size_t len) {
    std::memcpy(ptr, src, len);
    ptr += len;
}

inline void writeBytesAdvance(std::byte*& ptr, const uint8_t* src, size_t len) {
    std::memcpy(ptr, src, len);
    ptr += len;
}

template <std::unsigned_integral T>
T readIntegerAdvance(std::span<const std::byte>& view) {
    T val{};
    std::memcpy(&val, view.data(), sizeof(T));
    view = view.subspan(sizeof(T));
    return swapEndian(val);
}

inline void readBytesAdvance(std::span<const std::byte>& view, std::byte* out,
                             std::size_t len) {
    std::memcpy(out, view.data(), len);
    view = view.subspan(len);
}
