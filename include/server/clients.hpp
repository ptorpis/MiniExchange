#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>

#include <algorithm>
#include <optional>

using ApiKey = std::array<uint8_t, 16>;
using HMACKey = std::array<uint8_t, 32>;

struct Array16Hash {
    std::size_t operator()(const std::array<uint8_t, 16>& arr) const noexcept {
        std::size_t hash = 0;
        for (auto b : arr) {
            hash = hash * 31 + b;
        }
        return hash;
    }
};

using ClientMap = std::unordered_map<ApiKey, HMACKey, Array16Hash>;

class ClientManager {
public:
    ClientManager() = default;
    void addClient(ApiKey apiKey, HMACKey hmackey) {
        clientMap_.insert({apiKey, hmackey});
    }

    std::optional<HMACKey> getHMACKey(ApiKey apiKey) {
        auto it = clientMap_.find(apiKey);
        return (it != clientMap_.end()) ? std::optional<HMACKey>(it->second)
                                        : std::nullopt;
    }

    void addTestDefault() {
        ApiKey apikey{};
        HMACKey hmackey{};
        std::fill(apikey.begin(), apikey.end(), 0x22);
        std::fill(hmackey.begin(), hmackey.end(), 0x11);

        addClient(apikey, hmackey);
    }

private:
    ClientMap clientMap_;
};