#pragma once

#include <cstdint>
#include <utility>

enum class MDMsgType : std::uint8_t { DELTA = 0, SNAPSHOT = 1 };
enum class MDDeltatype : std::uint8_t { ADD = 0, REDUCE = 1 };

#pragma pack(push, 1)
struct MarketDataHeader {
    std::uint64_t sequenceNumber;
    std::uint32_t instrumentID;
    std::uint16_t payloadLength;
    std::uint8_t mdMsgType;
    std::uint8_t version;

private:
    template <typename F, typename Self>
    static void iterateElementsWithNames(Self& self, F&& func) {
        func("sequenceNumber", self.sequenceNumber);
        func("mdMsgType", self.mdMsgType);
        func("version", self.version);
        func("payloadLength", self.payloadLength);
        func("instrumentID", self.instrumentID);
    }

public:
    template <typename F> void iterateElements(F&& func) {
        iterateHelperWithNames(*this, [&](auto&&, auto& field) {
            func(field);
        });
    }

    template <typename F> void iterateElements(F&& func) const {
        iterateHelperWithNames(*this, [&](auto&&, const auto& field) {
            func(field);
        });
    }

    template <typename F> void iterateElementsWithNames(F&& func) {
        iterateHelperWithNames(*this, std::forward<F>(func));
    }

    template <typename F> void iterateElementsWithNames(F&& func) const {
        iterateHelperWithNames(*this, std::forward<F>(func));
    }

    struct traits {
        static constexpr std::size_t HEADER_SIZE = 16;
        static constexpr std::uint8_t PROTOCOL_VERSION = 0x01;
    };
};
#pragma pack(pop)
static_assert(sizeof(MarketDataHeader) == 16);

#pragma pack(push, 1)
struct DeltaPayload {
    std::uint64_t priceLevel;
    std::uint64_t amountDelta;
    std::uint8_t deltaType;
    std::uint8_t side;
    std::uint8_t _padding[6];

private:
    template <typename F, typename Self>
    static void iterateElementsWithNames(Self& self, F&& func) {
        func("priceLevel", self.priceLevel);
        func("amountDelta", self.amountDelta);
        func("deltaType", self.deltaType);
        func("side", self.side);
        func("_padding", self._padding);
    }

public:
    template <typename F> void iterateElements(F&& func) {
        iterateHelperWithNames(*this, [&](auto&&, auto& field) {
            func(field);
        });
    }

    template <typename F> void iterateElements(F&& func) const {
        iterateHelperWithNames(*this, [&](auto&&, const auto& field) {
            func(field);
        });
    }

    template <typename F> void iterateElementsWithNames(F&& func) {
        iterateHelperWithNames(*this, std::forward<F>(func));
    }

    template <typename F> void iterateElementsWithNames(F&& func) const {
        iterateHelperWithNames(*this, std::forward<F>(func));
    }

    struct traits {
        static constexpr std::size_t PAYLOAD_SIZE = 24;
    };
};
#pragma pack(pop)

static_assert(sizeof(DeltaPayload) == 24);

#pragma pack(push, 1)
struct SnapshotHeader {
    std::uint16_t bidCount;
    std::uint16_t askCount;
    std::uint32_t _padding;

private:
    template <typename F, typename Self>
    static void iterateElementsWithNames(Self& self, F&& func) {
        func("bidCount", self.bidCount);
        func("askCount", self.askCount);
        func("_padding", self._padding);
    }

public:
    template <typename F> void iterateElements(F&& func) {
        iterateHelperWithNames(*this, [&](auto&&, auto& field) {
            func(field);
        });
    }

    template <typename F> void iterateElements(F&& func) const {
        iterateHelperWithNames(*this, [&](auto&&, const auto& field) {
            func(field);
        });
    }

    template <typename F> void iterateElementsWithNames(F&& func) {
        iterateHelperWithNames(*this, std::forward<F>(func));
    }

    template <typename F> void iterateElementsWithNames(F&& func) const {
        iterateHelperWithNames(*this, std::forward<F>(func));
    }

    struct traits {
        static constexpr std::size_t SNAPSHOT_HEADER_SIZE = 8;
    };
};
#pragma pack(pop)

static_assert(sizeof(SnapshotHeader) == 8);

#pragma pack(push, 1)
struct SnapshotLevel {
    std::uint64_t price;
    std::uint64_t qty;

private:
    template <typename F, typename Self>
    static void iterateElementsWithNames(Self& self, F&& func) {
        func("price", self.price);
        func("qty", self.qty);
    }

public:
    template <typename F> void iterateElements(F&& func) {
        iterateHelperWithNames(*this, [&](auto&&, auto& field) {
            func(field);
        });
    }

    template <typename F> void iterateElements(F&& func) const {
        iterateHelperWithNames(*this, [&](auto&&, const auto& field) {
            func(field);
        });
    }

    template <typename F> void iterateElementsWithNames(F&& func) {
        iterateHelperWithNames(*this, std::forward<F>(func));
    }

    template <typename F> void iterateElementsWithNames(F&& func) const {
        iterateHelperWithNames(*this, std::forward<F>(func));
    }

    struct traits {
        static constexpr std::size_t LEVEL_SIZE = 16;
    };
};
#pragma pack(pop)

static_assert(sizeof(SnapshotLevel) == 16);
