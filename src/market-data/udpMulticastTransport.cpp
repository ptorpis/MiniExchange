#include "market-data/udpMulticastTransport.hpp"

#include <cstring>
#include <iostream>
#include <print>
#include <span>
#include <stdexcept>
#include <string>
#include <sys/socket.h>

using namespace market_data;

void UDPMulticastTransport::send(std::span<const std::byte> msgBytes) {
    if (sockfd_ < 0) {
        throw std::runtime_error("Socket not initialized");
    }

    ssize_t sent = ::sendto(sockfd_, msgBytes.data(), msgBytes.size(), 0,
                            reinterpret_cast<const sockaddr*>(&addr_), sizeof(addr_));

    if (sent < 0) {
        throw std::runtime_error("Failed to send UDP packet: " +
                                 std::string(strerror(errno)));
    }

    std::println("Sent bytes: {}", sent);

    if (static_cast<std::size_t>(sent) != msgBytes.size()) {
        throw std::runtime_error("Partial send: sent" + std::to_string(sent) +
                                 " bytes, expected " + std::to_string(msgBytes.size()));
    }
}

void UDPMulticastTransport::initialize() {
    sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_ < 0) {
        throw std::runtime_error("Failed to create socket: " +
                                 std::string(strerror(errno)));
    }

    if (setsockopt(sockfd_, IPPROTO_IP, IP_MULTICAST_TTL, &config_.ttl,
                   sizeof(config_.ttl)) < 0) {
        close(sockfd_);
        throw std::runtime_error("Failed to set multicast TTL: " +
                                 std::string(strerror(errno)));
    }

    int loopback = 1;
    if (setsockopt(sockfd_, IPPROTO_IP, IP_MULTICAST_LOOP, &loopback, sizeof(loopback)) <
        0) {
        close(sockfd_);
        throw std::runtime_error("Failed to enable multicast loopback: " +
                                 std::string(strerror(errno)));
    }

    if (config_.interfaceIP != "0.0.0.0") {
        in_addr localInterface{};
        localInterface.s_addr = inet_addr(config_.interfaceIP.c_str());
        if (setsockopt(sockfd_, IPPROTO_IP, IP_MULTICAST_IF, &localInterface,
                       sizeof(localInterface)) < 0) {
            close(sockfd_);
            throw std::runtime_error("Failed to set multicast interface: " +
                                     std::string(strerror(errno)));
        }
    }

    std::memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(config_.port);
    addr_.sin_addr.s_addr = inet_addr(config_.multicastGroup.c_str());

    std::cout << "Binding to port: " << config_.port << std::endl;
    std::cout << "Joining group: " << config_.multicastGroup << std::endl;
    std::cout << "Interface: " << config_.interfaceIP << std::endl;
}
