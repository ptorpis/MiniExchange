#pragma once

#include <cstddef>
#include <cstdint>

namespace constants {
inline static constexpr std::size_t HEADER_SIZE = 16;
enum class HeaderFlags : std::uint8_t { PROTOCOL_VERSION = 0x02 };
} // namespace constants

namespace client {
template <typename T> struct PayloadTraits;
}

namespace client {
template <typename T> struct PayloadTraits;
}

#pragma pack(push, 1)
struct MessageHeader {
    std::uint8_t messageType;
    std::uint8_t protocolVersionFlag;
    std::uint16_t payloadLength;
    std::uint32_t clientMsgSqn;
    std::uint32_t serverMsgSqn;
    std::uint8_t padding[4];

    template <typename F> void iterateElements(F&& func) {
        func(messageType);
        func(protocolVersionFlag);
        func(payloadLength);
        func(clientMsgSqn);
        func(serverMsgSqn);
        func(padding);
    }
};
#pragma pack(pop)

template <typename Payload> struct Message {
    MessageHeader header;
    Payload payload;
};
