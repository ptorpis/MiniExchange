#pragma once

#include "market-data/messages.hpp"
#include "utils/types.hpp"
#include <arpa/inet.h>
#include <cstddef>
#include <fcntl.h>
#include <functional>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

struct MDConfig {
    std::string multicastGroup = "239.0.0.1";
    std::uint16_t port = 9001;
    std::string interfaceIP = "0.0.0.0";
};

using OnSnapshotCallback =
    std::function<void(const Level2OrderBook&, std::uint64_t seqNum)>;
using OnDeltaCallback =
    std::function<void(Price, Qty, OrderSide, MDDeltatype, std::uint64_t seqNum)>;
using OnGapDetectedCallback =
    std::function<void(std::uint64_t expected, std::uint64_t received)>;
using OnBookValidCallback = std::function<void()>;
using OnBookInvalidCallback = std::function<void()>;

class MDReceiver {
public:
    MDReceiver(MDConfig mdcfg = MDConfig())
        : mdConfig_(mdcfg), mdBuffer_(16 * 1024), bookValid_(false) {}

    ~MDReceiver() {
        if (sockfd_ >= 0) {
            close(sockfd_);
        }
    }

    bool receiveOne();
    void initialize();

    const std::vector<std::pair<Price, Qty>>& getBook(OrderSide side) const;
    std::vector<std::pair<Price, Qty>>& getBook(OrderSide side);

    const Level2OrderBook& getOrderBook() const { return book_; }

    bool isBookValid() const { return bookValid_; }

    void setOnSnapshot(OnSnapshotCallback cb) { onSnapshot_ = std::move(cb); }
    void setOnDelta(OnDeltaCallback cb) { onDelta_ = std::move(cb); }
    void setOnGapDetected(OnGapDetectedCallback cb) { onGapDetected_ = std::move(cb); }
    void setOnBookValid(OnBookValidCallback cb) { onBookValid_ = std::move(cb); }
    void setOnBookInvalid(OnBookInvalidCallback cb) { onBookInvalid_ = std::move(cb); }

private:
    void processMessage_(std::span<const std::byte> msgBytes);
    MarketDataHeader parseHeader_(std::span<const std::byte>& hdrBytes);
    void processDelta_(std::span<const std::byte> payloadBytes, std::uint64_t sqn);
    void processSnapshot_(std::span<const std::byte> payloadBytes, std::uint64_t sqn);

    bool priceBetterOrEqual_(Price incoming, Price resting, OrderSide side);
    void addAtPrice_(Price price, Qty amount, OrderSide side);
    void reduceAtPrice_(Price price, Qty amount, OrderSide side);

    void checkSequence_(std::uint64_t receivedSqn);
    void handleGap_(std::uint64_t expected, std::uint64_t received);
    void markBookValid();
    void markBookInvalid();

    MDConfig mdConfig_;
    std::vector<std::byte> mdBuffer_;

    Level2OrderBook book_;
    bool bookValid_;

    std::optional<std::uint64_t> expectedMDSqn_;
    int sockfd_{-1};

    OnSnapshotCallback onSnapshot_;
    OnDeltaCallback onDelta_;
    OnGapDetectedCallback onGapDetected_;
    OnBookValidCallback onBookValid_;
    OnBookInvalidCallback onBookInvalid_;
};
