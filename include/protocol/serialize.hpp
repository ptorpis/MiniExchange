#pragma once

#include "protocol/messages.hpp"
#include "protocol/protocolTypes.hpp"
#include "utils/endian.hpp"
#include "utils/types.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>

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

    std::memcpy(&msg.payload, view.data(), payloadSize);

    msg.payload.iterateElements([](auto& field) {
        swapFieldEndian(field);
    });

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
    payloadBE.iterateElements([](auto& field) {
        swapFieldEndian(field);
    });
    std::memcpy(ptr, &payloadBE, payloadSize);

    return msg;
}

template <typename Payload>
void serializeMessageInto(std::vector<std::byte>& buffer, MessageType msgType,
                          const MessageHeader& header, const Payload& payload) {
    constexpr size_t msgSize = sizeof(MessageHeader) + sizeof(Payload);

    size_t oldSize = buffer.size();
    buffer.resize(oldSize + msgSize);

    std::byte* ptr = buffer.data() + oldSize;

    writeByteAdvance(ptr, static_cast<std::byte>(msgType));
    writeByteAdvance(ptr, static_cast<std::byte>(header.protocolVersionFlag));
    writeIntegerAdvance(ptr, header.payloadLength);
    writeIntegerAdvance(ptr, header.clientMsgSqn);
    writeIntegerAdvance(ptr, header.serverMsgSqn);
    writeBytesAdvance(ptr, header.padding, sizeof(header.padding));

    Payload payloadBE = payload;
    payloadBE.iterateElements([](auto& field) {
        swapFieldEndian(field);
    });
    std::memcpy(ptr, &payloadBE, sizeof(Payload));
}
