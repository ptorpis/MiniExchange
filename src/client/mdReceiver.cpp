#include "client/mdReceiver.hpp"
#include "market-data/messages.hpp"
#include "utils/endian.hpp"
#include "utils/types.hpp"
#include "utils/utils.hpp"

#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void MDReceiver::initialize() {
    sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_ < 0) {
        throw std::runtime_error("Failed to create socket: " +
                                 std::string(strerror(errno)));
    }

    int flags = fcntl(sockfd_, F_GETFL, 0);
    if (flags < 0) {
        close(sockfd_);
        throw std::runtime_error("Failed to get socket flags: " +
                                 std::string(strerror(errno)));
    }
    if (fcntl(sockfd_, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(sockfd_);
        throw std::runtime_error("Failed to set non-blocking: " +
                                 std::string(strerror(errno)));
    }

    int reuse = 1;
    if (setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        close(sockfd_);
        throw std::runtime_error("Failed to set SO_REUSEADDR: " +
                                 std::string(strerror(errno)));
    }

    int loopback = 1;
    if (setsockopt(sockfd_, IPPROTO_IP, IP_MULTICAST_LOOP, &loopback, sizeof(loopback)) <
        0) {
        close(sockfd_);
        throw std::runtime_error("Failed to enable multicast loopback: " +
                                 std::string(strerror(errno)));
    }

    sockaddr_in localAddr{};
    std::memset(&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(mdConfig_.port);
    localAddr.sin_addr.s_addr = inet_addr(mdConfig_.multicastGroup.c_str());

    if (bind(sockfd_, reinterpret_cast<sockaddr*>(&localAddr), sizeof(localAddr)) < 0) {
        close(sockfd_);
        throw std::runtime_error("Failed to bind socket: " +
                                 std::string(strerror(errno)));
    }

    ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = inet_addr(mdConfig_.multicastGroup.c_str());
    mreq.imr_interface.s_addr = inet_addr(mdConfig_.interfaceIP.c_str());

    if (setsockopt(sockfd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        close(sockfd_);
        throw std::runtime_error("Failed to join multicast group: " +
                                 std::string(strerror(errno)));
    }

    std::cout << "MDReceiver initialized: " << mdConfig_.multicastGroup << ":"
              << mdConfig_.port << std::endl;

    std::cout << "Binding to port: " << mdConfig_.port << std::endl;
    std::cout << "Joining group: " << mdConfig_.multicastGroup << std::endl;
    std::cout << "Interface: " << mdConfig_.interfaceIP << std::endl;
}

bool MDReceiver::receiveOne() {

    ssize_t bytesReceived =
        ::recvfrom(sockfd_, mdBuffer_.data(), mdBuffer_.size(), 0, nullptr, nullptr);

    if (bytesReceived < 0) {
        if (errno == EAGAIN) {
            return false;
        }
        std::cerr << "Error receiving data: " << strerror(errno) << std::endl;
        return false;
    }

    if (bytesReceived == 0) {
        std::cout << "Received 0-byte packet" << std::endl;
        return false;
    }

    std::cout << "Bytes received: " << bytesReceived << std::endl;

    if (bytesReceived < 16) {
        return false;
    }
    processMessage_(std::span<const std::byte>(mdBuffer_.data(),
                                               static_cast<std::size_t>(bytesReceived)));
    return true;
}
void MDReceiver::processMessage_(std::span<const std::byte> msgBytes) {
    utils::printHex(msgBytes);
    if (msgBytes.size() < 16) {
        return;
    }

    auto originalData = msgBytes;
    MarketDataHeader header = parseHeader_(msgBytes);

    checkSequence_(header.sequenceNumber);

    auto payload = originalData.subspan(MarketDataHeader::traits::HEADER_SIZE);
    auto msgType = static_cast<MDMsgType>(header.mdMsgType);

    if (msgType == MDMsgType::DELTA) {
        processDelta_(payload, header.sequenceNumber);
    } else if (msgType == MDMsgType::SNAPSHOT) {
        processSnapshot_(payload, header.sequenceNumber);
    }
}

MarketDataHeader MDReceiver::parseHeader_(std::span<const std::byte>& hdrBytes) {
    MarketDataHeader header{};
    header.sequenceNumber = readIntegerAdvance<std::uint64_t>(hdrBytes);
    header.instrumentID = readIntegerAdvance<std::uint32_t>(hdrBytes);
    header.payloadLength = readIntegerAdvance<std::uint16_t>(hdrBytes);
    header.mdMsgType = readByteAdvance(hdrBytes);
    header.version = readByteAdvance(hdrBytes);

    return header;
}

void MDReceiver::processDelta_(std::span<const std::byte> payloadBytes,
                               std::uint64_t sqn) {
    if (payloadBytes.size() < 24) {
        std::cerr << "Delta payload too small" << std::endl;
        return;
    }
    if (!bookValid_) {
        return;
    }

    DeltaPayload delta{};
    delta.priceLevel = readIntegerAdvance<std::uint64_t>(payloadBytes);
    delta.amountDelta = readIntegerAdvance<std::uint64_t>(payloadBytes);
    delta.deltaType = readByteAdvance(payloadBytes);
    delta.side = readByteAdvance(payloadBytes);

    if (delta.deltaType == +MDDeltatype::ADD) {
        addAtPrice_(Price{delta.priceLevel}, Qty{delta.amountDelta},
                    OrderSide{delta.side});
    } else {
        reduceAtPrice_(Price{delta.priceLevel}, Qty{delta.amountDelta},
                       OrderSide{delta.side});
    }

    if (onDelta_) {
        onDelta_(Price{delta.priceLevel}, Qty{delta.amountDelta}, OrderSide{delta.side},
                 MDDeltatype{delta.deltaType}, sqn);
    }
}

void MDReceiver::processSnapshot_(std::span<const std::byte> payloadBytes,
                                  std::uint64_t sqn) {
    if (payloadBytes.size() < 8) {
        std::cerr << "Snapshot payload too small" << std::endl;
        return;
    }

    std::uint16_t bidCount = readIntegerAdvance<std::uint16_t>(payloadBytes);
    std::uint16_t askCount = readIntegerAdvance<std::uint16_t>(payloadBytes);
    std::uint32_t _ = readIntegerAdvance<std::uint32_t>(payloadBytes); // padding

    book_.bids.clear();
    book_.asks.clear();

    for (std::uint16_t i = 0; i < bidCount; ++i) {
        if (payloadBytes.size() < 16) {
            break;
        }

        std::uint64_t price = readIntegerAdvance<std::uint64_t>(payloadBytes);
        std::uint64_t qty = readIntegerAdvance<std::uint64_t>(payloadBytes);

        book_.bids.emplace_back(Price{price}, Qty{qty});
    }

    for (std::uint16_t i = 0; i < askCount; ++i) {
        if (payloadBytes.size() < 16) {
            break;
        }

        std::uint64_t price = readIntegerAdvance<std::uint64_t>(payloadBytes);
        std::uint64_t qty = readIntegerAdvance<std::uint64_t>(payloadBytes);

        book_.asks.emplace_back(Price{price}, Qty{qty});
    }

    markBookValid();

    if (onSnapshot_) {
        onSnapshot_(book_, sqn);
    }
}

void MDReceiver::checkSequence_(std::uint64_t receivedSeqNum) {
    if (!expectedMDSqn_.has_value()) {
        // First message - accept any sequence number
        expectedMDSqn_ = receivedSeqNum + 1;
        return;
    }

    if (receivedSeqNum != *expectedMDSqn_) {
        handleGap_(*expectedMDSqn_, receivedSeqNum);
    }

    expectedMDSqn_ = receivedSeqNum + 1;
}

void MDReceiver::handleGap_(std::uint64_t expected, std::uint64_t received) {
    std::cerr << "Gap detected: expected " << expected << "; received " << received
              << " (missed " << (received - expected) << " messages)" << std::endl;

    markBookInvalid();

    if (onGapDetected_) {
        onGapDetected_(expected, received);
    }
}

void MDReceiver::markBookValid() {
    if (!bookValid_) {
        bookValid_ = true;
        if (onBookValid_) {
            onBookValid_();
        }
    }
}

void MDReceiver::markBookInvalid() {
    if (bookValid_) {
        bookValid_ = false;
        if (onBookInvalid_) {
            onBookInvalid_();
        }
    }
}

const std::vector<std::pair<Price, Qty>>& MDReceiver::getBook(OrderSide side) const {
    return side == OrderSide::BUY ? book_.bids : book_.asks;
}

std::vector<std::pair<Price, Qty>>& MDReceiver::getBook(OrderSide side) {
    return side == OrderSide::BUY ? book_.bids : book_.asks;
}

bool MDReceiver::priceBetterOrEqual_(Price incoming, Price resting, OrderSide side) {
    if (side == OrderSide::BUY) {
        return incoming >= resting;
    } else {
        return resting >= incoming;
    }
}

void MDReceiver::addAtPrice_(Price price, Qty amount, OrderSide side) {
    auto& book = getBook(side);

    auto it = std::find_if(book.rbegin(), book.rend(), [&](const auto& level) {
        return level.first == price || !priceBetterOrEqual_(price, level.first, side);
    });

    if (it != book.rend() && it->first == price) {
        it->second += amount;
        return;
    }

    auto insertIt = (it == book.rend()) ? book.begin() : it.base();
    book.insert(insertIt, {price, amount});
}

void MDReceiver::reduceAtPrice_(Price price, Qty amount, OrderSide side) {
    auto& book = getBook(side);
    for (auto rIt = book.rbegin(); rIt != book.rend(); ++rIt) {
        if (rIt->first == price) {
            rIt->second -= amount;
            if (rIt->second == Qty{0}) {
                book.erase(std::next(rIt).base());
            }
            return;
        }
    }
}
