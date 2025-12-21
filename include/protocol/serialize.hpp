#pragma once

#include "protocol/messages.hpp"
#include "protocol/protocolTypes.hpp"
#include "utils/types.hpp"
#include <bit>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <optional>
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

    if constexpr (requires {
                      field.value();
                      field.data_m;
                  }) {
        using UnderlyingType = decltype(field.data_m);
        if constexpr (std::is_integral_v<UnderlyingType> && sizeof(UnderlyingType) > 1) {
            field.data_m = swapEndian(field.data_m);
        }
    } else if constexpr (std::is_integral_v<FieldType> && sizeof(FieldType) > 1) {
        field = swapEndian(field);
    }
}

inline std::byte readByteAdvance(std::span<const std::byte>& view) {
    std::byte value = view.front();
    view = view.subspan(1);
    return value;
}

inline uint8_t readUint8Advance(std::span<const std::byte>& view) {
    return std::to_integer<uint8_t>(readByteAdvance(view));
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

template <typename Payload>
std::optional<Message<Payload>> deserializeMessage(std::span<const std::byte> buffer) {
    constexpr std::size_t headerSize = sizeof(MessageHeader);
    constexpr std::size_t payloadSize = sizeof(Payload);
    if (buffer.size() < headerSize + payloadSize) {
        return std::nullopt;
    }
    Message<Payload> msg{};

    auto view = buffer;
    msg.header.messageType = readByteAdvance(view);
    msg.header.protocolVersionFlag = readByteAdvance(view);
    msg.header.payloadLength = readIntegerAdvance<std::uint16_t>(view);
    msg.header.clientMsgSqn = readIntegerAdvance<std::uint32_t>(view);
    msg.header.serverMsgSqn = readIntegerAdvance<std::uint32_t>(view);
    readBytesAdvance(view, reinterpret_cast<std::byte*>(msg.header.padding),
                     sizeof(msg.header.padding));

    msg.payload.iterateElements([](auto& field) { field = swapFieldEndian(field); });

    return msg;
}

template <typename Payload>
SerializedMessage serializeMessage(const MessageType msgType, const MessageHeader& header,
                                   const Payload& payload) {
    constexpr std::size_t headerSize = sizeof(MessageHeader);
    constexpr std::size_t payloadSize = sizeof(Payload);
    constexpr std::size_t messageSize = headerSize + payloadSize;

    SerializedMessage msg;
    msg.buffer.resize(messageSize);
    std::byte* ptr = msg.buffer.data();

    writeByteAdvance(ptr, static_cast<std::byte>(msgType));
    writeByteAdvance(ptr, static_cast<std::byte>(header.protocolVersionFlag));
    writeIntegerAdvance(ptr, header.payloadLength);
    writeIntegerAdvance(ptr, header.clientMsgSqn);
    writeIntegerAdvance(ptr, header.serverMsgSqn);
    writeBytesAdvance(ptr, header.padding, sizeof(header.padding));

    Payload payloadBE = payload;
    payloadBE.iterateElements([](auto& field) { swapFieldEndian(field); });
    std::memcpy(ptr, &payloadBE, payloadSize);

    return msg;
}
