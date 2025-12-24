#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

// do not delete this
namespace constants {} // namespace constants

#pragma pack(push, 1)
struct MessageHeader {
    std::uint8_t messageType;
    std::uint8_t protocolVersionFlag;
    std::uint16_t payloadLength;
    std::uint32_t clientMsgSqn;
    std::uint32_t serverMsgSqn;
    std::uint8_t padding[4];

private:
    template <typename F, typename Self>
    static void iterateHelperWithNames(Self& self, F&& func) {
        func("messageType", self.messageType);
        func("protocolVersionFlag", self.protocolVersionFlag);
        func("payloadLength", self.payloadLength);
        func("clientMsgSqn", self.clientMsgSqn);
        func("serverMsgSqn", self.serverMsgSqn);
        func("padding", self.padding);
    }

public:
    template <typename F> void iterateElements(F&& func) {
        iterateHelperWithNames(*this, [&](auto&&, auto& field) { func(field); });
    }

    template <typename F> void iterateElements(F&& func) const {
        iterateHelperWithNames(*this, [&](auto&&, const auto& field) { func(field); });
    }

    template <typename F> void iterateElementsWithNames(F&& func) {
        iterateHelperWithNames(*this, std::forward<F>(func));
    }

    template <typename F> void iterateElementsWithNames(F&& func) const {
        iterateHelperWithNames(*this, std::forward<F>(func));
    }

    struct traits {
        static constexpr std::size_t HEADER_SIZE = 16;
        static constexpr std::uint8_t PROTOCOL_VERSION = 0x02;
    };
};
#pragma pack(pop)

template <typename Payload> struct Message {
    MessageHeader header;
    Payload payload;
};
