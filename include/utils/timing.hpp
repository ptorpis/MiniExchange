#pragma once

#include <chrono>
#include <cstdint>

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
