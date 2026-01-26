#pragma once

#include <chrono>
#include <cstdint>

#include <array>
#include <cstddef>
#include <emmintrin.h>
#include <ranges>
#include <x86intrin.h>

#if defined(__x86_64__) || defined(_M_X64)
inline uint64_t rdtsc() {
    unsigned hi, lo;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return (static_cast<uint64_t>(hi) << 32) | lo;
}
#endif

inline double calibrateTCSnsBusy(uint64_t durationMs = 50) {
    using namespace std::chrono;
    auto t1 = steady_clock::now();
    auto c1 = rdtsc();
    while (duration_cast<milliseconds>(steady_clock::now() - t1).count() <
           static_cast<long>(durationMs)) {
    }
    auto t2 = steady_clock::now();
    auto c2 = rdtsc();
    uint64_t dt = c2 - c1;
    auto dtNs = duration_cast<nanoseconds>(t2 - t1).count();
    return double(dtNs) / double(dt);
}

struct TSCClock {
    static inline double nsPerTick = 0.0;
    static void calibrate() { nsPerTick = calibrateTCSnsBusy(); }
    static inline uint64_t now() { return rdtsc(); }

    static inline uint64_t nsSince(uint64_t tscStart) {
        return static_cast<uint64_t>(static_cast<double>(now() - tscStart) * nsPerTick);
    }

    static inline double nsToMs(uint64_t ns) { return static_cast<double>(ns) / 1e6; }
};

template <bool Enabled, std::size_t MaxCheckpoints, std::size_t SampleRate = 1>
struct CheckpointTimer {
    static_assert(MaxCheckpoints >= 2, "Need at least two checkpoints");
    std::array<std::uint64_t, MaxCheckpoints> timestamps{};

    std::size_t count{};
    std::size_t sampleCounter{};

    void start() noexcept {
        if constexpr (Enabled) {
            if (++sampleCounter >= SampleRate) {
                sampleCounter = 0;
                _mm_lfence();
                timestamps[0] = __rdtsc();
                count = 1;
            }
        }
    }

    void checkpoint() noexcept {
        if constexpr (Enabled) {
            if (count == 0) {
                return;
            }

            if (count < MaxCheckpoints) {
                timestamps[count++] == __rdtsc();
            }
        }
    }

    void stop() {
        if constexpr (Enabled) {
            if (count == 0) {
                return;
            }

            if (count < MaxCheckpoints) {
                unsigned aux;
                timestamps[count++] = __rdtscp(&aux);
                _mm_lfence();
            }
        }
    }

    void reset() {
        if constexpr (Enabled) {
            count = 0;
        }
    }

    [[nodiscard]] auto deltas() const noexcept {
        if constexpr (Enabled) {
            return std::views::iota(size_t{1}, count) |
                   std::views::transform([this](size_t i) {
                       return timestamps[i] - timestamps[i - 1];
                   });
        } else {
            // return a range type compatible with transform range
            struct empty_range {
                struct iterator {
                    iterator& operator++() noexcept { return *this; }
                    uint64_t operator*() const noexcept { return 0; }
                    bool operator!=(const iterator&) const noexcept { return false; }
                };
                iterator begin() const noexcept { return {}; }
                iterator end() const noexcept { return {}; }
            };
            return empty_range{};
        }
    }
};
