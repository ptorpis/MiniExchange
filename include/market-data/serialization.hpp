#pragma once

#include "market-data/messages.hpp"
#include "utils/endian.hpp"
#include "utils/types.hpp"
#include "utils/utils.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace market_data {

inline std::array<std::byte, 16> serializeHeader(const MarketDataHeader& header) {
    std::array<std::byte, 16> buffer{};
    std::byte* ptr = buffer.data();

    writeIntegerAdvance(ptr, header.sequenceNumber);
    writeIntegerAdvance(ptr, header.instrumentID);
    writeIntegerAdvance(ptr, header.payloadLength);
    writeByteAdvance(ptr, static_cast<std::byte>(header.mdMsgType));
    writeByteAdvance(ptr, static_cast<std::byte>(header.version));

    return buffer;
}

inline std::array<std::byte, 24> serializeDelta(const DeltaPayload& delta) {
    std::array<std::byte, 24> buffer{};
    std::byte* ptr = buffer.data();

    writeIntegerAdvance(ptr, delta.priceLevel);
    writeIntegerAdvance(ptr, delta.amountDelta);
    writeByteAdvance(ptr, static_cast<std::byte>(delta.deltaType));
    writeByteAdvance(ptr, static_cast<std::byte>(delta.side));

    writeBytesAdvance(ptr, delta._padding, sizeof(delta._padding));

    return buffer;
}

inline std::array<std::byte, 8>
serializeSnapshotHeader(const SnapshotHeader& snapHeader) {
    std::array<std::byte, 8> buffer{};
    std::byte* ptr = buffer.data();

    writeIntegerAdvance(ptr, snapHeader.bidCount);
    writeIntegerAdvance(ptr, snapHeader.askCount);
    writeIntegerAdvance(ptr, snapHeader._padding);

    return buffer;
}

inline std::array<std::byte, 16> serializeLevel(const SnapshotLevel& level) {
    std::array<std::byte, 16> buffer{};
    std::byte* ptr = buffer.data();

    writeIntegerAdvance(ptr, level.price);
    writeIntegerAdvance(ptr, level.qty);

    return buffer;
}

inline std::array<std::byte, 40> serializeDeltaMessage(std::uint64_t sequenceNumber,
                                                       std::uint32_t instrumentID,
                                                       const DeltaPayload& delta) {
    std::array<std::byte, 40> buffer{};
    MarketDataHeader header{};

    header.sequenceNumber = sequenceNumber;
    header.instrumentID = instrumentID;
    header.payloadLength = DeltaPayload::traits::PAYLOAD_SIZE;
    header.mdMsgType = +MDMsgType::DELTA;
    header.version = MarketDataHeader::traits::PROTOCOL_VERSION;

    auto headerBytes = serializeHeader(header);
    std::memcpy(buffer.data(), headerBytes.data(), headerBytes.size());

    auto payloadBytes = serializeDelta(delta);
    std::memcpy(buffer.data() + MarketDataHeader::traits::HEADER_SIZE,
                payloadBytes.data(), payloadBytes.size());

    return buffer;
}

inline std::vector<std::byte>
serializeSnapshotMessage(std::uint64_t sequenceNumber, std::uint32_t instrumentID,
                         const std::vector<std::pair<Price, Qty>>& bids,
                         const std::vector<std::pair<Price, Qty>>& asks,
                         std::size_t maxDepth) {
    std::uint16_t bidCount = static_cast<std::uint16_t>(std::min(bids.size(), maxDepth));
    std::uint16_t askCount = static_cast<std::uint16_t>(std::min(asks.size(), maxDepth));

    std::size_t payloadSize = SnapshotHeader::traits::SNAPSHOT_HEADER_SIZE +
                              (bidCount + askCount) * SnapshotLevel::traits::LEVEL_SIZE;
    std::size_t totalSize = MarketDataHeader::traits::HEADER_SIZE + payloadSize;

    std::vector<std::byte> buffer(totalSize);
    std::byte* ptr = buffer.data();

    MarketDataHeader header{};
    header.sequenceNumber = sequenceNumber;
    header.instrumentID = instrumentID;
    header.payloadLength = static_cast<std::uint16_t>(payloadSize);
    header.mdMsgType = +(MDMsgType::SNAPSHOT);
    header.version = MarketDataHeader::traits::PROTOCOL_VERSION;

    auto headerBytes = serializeHeader(header);
    writeBytesAdvance(ptr, headerBytes.data(), MarketDataHeader::traits::HEADER_SIZE);

    SnapshotHeader snapshotHeader{};
    snapshotHeader.bidCount = bidCount;
    snapshotHeader.askCount = askCount;
    snapshotHeader._padding = 0;

    auto snapHeaderBytes = serializeSnapshotHeader(snapshotHeader);
    writeBytesAdvance(ptr, snapHeaderBytes.data(),
                      SnapshotHeader::traits::SNAPSHOT_HEADER_SIZE);

    for (std::uint16_t i = 0; i < bidCount; ++i) {
        SnapshotLevel level{};
        level.price = bids[i].first.value();
        level.qty = bids[i].second.value();

        auto levelBytes = serializeLevel(level);
        writeBytesAdvance(ptr, levelBytes.data(), SnapshotLevel::traits::LEVEL_SIZE);
    }

    for (std::uint16_t i = 0; i < askCount; ++i) {
        SnapshotLevel level{};
        level.price = asks[i].first.value();
        level.qty = asks[i].second.value();
        auto levelBytes = serializeLevel(level);
        writeBytesAdvance(ptr, levelBytes.data(), SnapshotLevel::traits::LEVEL_SIZE);
    }

    return buffer;
}

} // namespace market_data
