#pragma once

#include "types.hpp"
#include <chrono>
#include <random>

class RandomGenerator {
public:
    RandomGenerator(uint64_t seed = 0) : rng_(seed) {}

    std::chrono::milliseconds jitter(int maxMillis) {
        std::uniform_int_distribution<int> dist(1, maxMillis);
        return std::chrono::milliseconds(dist(rng_));
    }

    Qty randomQty(Qty min, Qty max) {
        std::uniform_int_distribution<Qty> dist(min, max);
        return dist(rng_);
    }

    Price randomPrice(Price min, Price max) {
        std::uniform_int_distribution<Price> dist(min, max);
        return dist(rng_);
    }

private:
    std::mt19937_64 rng_;
};