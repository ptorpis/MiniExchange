#pragma once

#include "protocol/messages.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <type_traits>

// use with 16, 32, 64 bit integer types, requires C++ 23
template <typename T> inline T swapEndian(T val) {
    if constexpr (std::endian::native == std::endian::little) {
        return std::byteswap(val);
    }
    return val;
}

inline uint8_t readUint8Advance(const uint8_t*& ptr) {
    uint8_t val = *ptr;
    ++ptr;
    return val;
}

template <typename T> T readIntegerAdvance(const uint8_t*& ptr) {
    static_assert(std::is_integral_v<T>, "T must be integral");
    T val{};
    std::memcpy(&val, ptr, sizeof(T));
    ptr += sizeof(T);

    return swapEndian(val); // use your helper
}

inline void readBytesAdvance(const uint8_t*& ptr, uint8_t* out, size_t len) {
    std::memcpy(out, ptr, len);
    ptr += len;
}

inline void writeUint8Advance(uint8_t*& ptr, uint8_t val) {
    *ptr = val;
    ++ptr;
}

template <typename T> void writeIntegerAdvance(uint8_t*& ptr, T val) {
    static_assert(std::is_integral_v<T>, "T must be integral");

    T beVal = swapEndian(val);
    std::memcpy(ptr, &beVal, sizeof(T));
    ptr += sizeof(T);
}

inline void writeBytesAdvance(uint8_t*& ptr, const uint8_t* src, size_t len) {
    std::memcpy(ptr, src, len);
    ptr += len;
}

template <typename Payload>
std::vector<uint8_t> serializeMessage(MessageType msgType, const Payload& payload,
                                      const MessageHeader& header) {
    constexpr size_t headerSize = sizeof(MessageHeader);
    constexpr size_t payloadSize = sizeof(Payload);
    constexpr size_t totalSize = headerSize + payloadSize;

    std::vector<uint8_t> buffer(totalSize);
    uint8_t* ptr = buffer.data();

    {
        using namespace constants::HeaderOffset;
        writeUint8Advance(ptr, static_cast<uint8_t>(msgType));
        writeUint8Advance(ptr, header.protocolVersionFlag);
        writeBytesAdvance(ptr, header.reservedFlags, sizeof(header.reservedFlags));
        writeIntegerAdvance(ptr, header.payLoadLength);
        writeIntegerAdvance(ptr, header.clientMsgSqn);
        writeIntegerAdvance(ptr, header.serverMsgSqn);
        writeBytesAdvance(ptr, header.padding, sizeof(header.padding));
    }

    Payload payloadBE = payload;
    payloadBE.iterateElements([](auto& field) {
        using FieldType = std::decay_t<decltype(field)>;
        if constexpr (std::is_integral_v<FieldType> && sizeof(FieldType) > 1) {
            field = swapEndian(field);
        }
    });

    std::memcpy(ptr, &payloadBE, payloadSize);

    return buffer;
}

template <typename Payload>
std::optional<Message<Payload>> deserializeMessage(std::span<const uint8_t> buffer) {
    constexpr size_t headerSize = sizeof(MessageHeader);
    constexpr size_t payloadSize = sizeof(Payload);

    if (buffer.size() < headerSize + payloadSize) {
        return std::nullopt; // not enough data
    }

    Message<Payload> msg{};
    const uint8_t* ptr = buffer.data();

    {
        using namespace constants::HeaderOffset;
        msg.header.messageType = readUint8Advance(ptr);
        msg.header.protocolVersionFlag = readUint8Advance(ptr);
        readBytesAdvance(ptr, msg.header.reservedFlags, sizeof(msg.header.reservedFlags));
        msg.header.payLoadLength = readIntegerAdvance<uint16_t>(ptr);
        msg.header.clientMsgSqn = readIntegerAdvance<uint32_t>(ptr);
        msg.header.serverMsgSqn = readIntegerAdvance<uint32_t>(ptr);
        readBytesAdvance(ptr, msg.header.padding, sizeof(msg.header.padding));
    }

    std::memcpy(&msg.payload, ptr, payloadSize);

    msg.payload.iterateElements([](auto& field) {
        using FieldType = std::decay_t<decltype(field)>;
        if constexpr (std::is_integral_v<FieldType> && sizeof(FieldType) > 1) {
            field = swapEndian(field);
        }
    });

    return msg;
}
