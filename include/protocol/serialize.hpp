#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "protocol/messages.hpp"

// use with 16, 32, 64 bit integer types, requires C++ 23
template <typename T> inline T swapEndian(T val) {
    if constexpr (std::endian::native == std::endian::little) {
        return std::byteswap(val);
    }
    return val;
}

inline void writeUint8(uint8_t* buf, size_t offset, uint8_t val) {
    buf[offset] = val;
}

inline uint8_t readUint8(const uint8_t* buf, size_t offset) {
    return buf[offset];
}

template <typename T> inline void writeInteger(uint8_t* buf, size_t offset, T val) {
    val = swapEndian(val);
    std::memcpy(buf + offset, &val, sizeof(T));
}

template <typename T> inline T readInteger(const uint8_t* buf, size_t offset) {
    T val;
    std::memcpy(&val, buf + offset, sizeof(T));
    return swapEndian(val);
}

inline void writeBytes(uint8_t* buf, size_t offset, const uint8_t* src, size_t len) {
    std::memcpy(buf + offset, src, len);
}

inline void readBytes(const uint8_t* buf, size_t offset, uint8_t* dst, size_t len) {
    std::memcpy(dst, buf + offset, len);
}

template <typename Payload> std::array<uint8_t, sizeof(MessageHeader) + sizeof(Payload)>
serializeMessage(MessageType msgType, const Payload& payload,
                 const MessageHeader& header) {

    constexpr size_t payloadSize = sizeof(Payload);
    constexpr size_t headerSize = sizeof(MessageHeader);
    constexpr size_t totalSize = headerSize + payloadSize;

    std::array<uint8_t, totalSize> buffer{};
    uint8_t* ptr = buffer.data();

    {
        using namespace Offset = constants::HeaderOffset;
        writeUint8(ptr, Offset::MESSAGE_TYPE, static_cast<uint8_t>(msgType));
        writeUint8(ptr, Offset::PROTOCOL_VERSION_FLAG, header.protocolVersionFlag);
        writeBytes(ptr, Offset::RESERVED_FLAGS, header.reservedFlags,
                   sizeof(header.reservedFlags));
        writeInteger(ptr, Offset::PAYLOAD_LENGTH, header.payLoadLength);
        writeInteger(ptr, Offset::CLIENT_MSG_SQN, header.clientMsgSqn);
        writeInteger(ptr, Offset::SERVER_MSG_SQN, header.serverMsgSqn);
        writeBytes(ptr, Offset::PADDING, header.padding, sizeof(header.padding));
    }

    Payload payloadBE = payload;
    payloadBE.iterateElements([](auto& field) {
        using FieldType = std::decay_t<decltype(field)>;
        if constexpr (std::is_integral_v<FieldType> && sizeof(FieldType) > 1) {
            field = swapEndian(field);
        }
    });

    std::memcpy(ptr + headerSize, &payloadBE, payloadSize);

    return buffer;
}

template <typename Payload> Message<Payload> deserializeMessage(const uint8_t* buffer) {
    Message<Payload> msg{};
    constexpr size_t headerSize = sizeof(MessageHeader);
    constexpr size_t payloadSize = sizeof(Payload);

    const uint8_t* ptr = buffer;

    {
        using namespace Offset = constants::HeaderOffset;
        msg.header.messageType = readUint8(ptr, Offset::MESSAGE_TYPE);
        msg.header.protocolVersionFlag = readUint8(ptr, Offset::PROTOCOL_VERSION_FLAG);
        readBytes(ptr, Offset::RESERVED_FLAGS, msg.header.reservedFlags,
                  sizeof(msg.header.reservedFlags));
        msg.header.payLoadLength = readInteger<uint16_t>(ptr, Offset::PAYLOAD_LENGTH);
        msg.header.clientMsgSqn = readInteger<uint32_t>(ptr, Offset::CLIENT_MSG_SQN);
        msg.header.serverMsgSqn = readInteger<uint32_t>(ptr, Offset::SERVER_MSG_SQN);
        readBytes(ptr, Offset::PADDING, msg.header.padding, sizeof(msg.header.padding));
    }

    std::memcpy(&msg.payload, ptr + headerSize, payloadSize);

    msg.payload.iterateElements([](auto& field) {
        using FieldType = std::decay_t<decltype(field)>;
        if constexpr (std::is_integral_v<FieldType> && sizeof(FieldType) > 1) {
            field = swapEndian(field);
        }
    });

    return msg;
}
