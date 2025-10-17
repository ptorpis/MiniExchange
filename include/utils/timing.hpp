#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

#if defined(__x86_64__) || defined(_M_X64)
inline uint64_t rdtsc() {
    unsigned hi, lo;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
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
        return uint64_t((now() - tscStart) * nsPerTick);
    }
    static inline double nsToMs(uint64_t ns) { return ns / 1e6; }
};

#ifdef ENABLE_TIMING

struct TimingRecord {
    uint64_t tReceived{0};
    uint64_t tDeserialized{0};
    uint64_t tBuffered{0};
    uint8_t messageType{0};
};

constexpr size_t MAX_TIMING_RECORDS = 8192;

inline std::array<TimingRecord, MAX_TIMING_RECORDS> timingBuffer;
inline std::atomic<size_t> timingIndex{0};

namespace utils {
inline void recordTiming(const TimingRecord& rec) {
    size_t i = timingIndex.fetch_add(1, std::memory_order_relaxed);
    if (i < MAX_TIMING_RECORDS) {
        timingBuffer[i] = rec;
    }
}

inline void startTimingConsumer(const char* filename = "timings.csv") {
    std::thread([filename] {
        FILE* f = fopen(filename, "w");
        if (!f) return;

        setvbuf(f, nullptr, _IOFBF, 1 << 20);

        fprintf(f, "messageType,tReceived,tDeserialized,tBuffered\n");

        size_t lastIndex = 0;

        while (true) {
            size_t currentIndex = timingIndex.load(std::memory_order_acquire);

            if (currentIndex > lastIndex) {
                for (size_t i = lastIndex; i < currentIndex; ++i) {
                    const auto& rec = timingBuffer[i];
                    fprintf(f, "%u,%lu,%lu,%lu\n", rec.messageType, rec.tReceived,
                            rec.tDeserialized, rec.tBuffered);
                }
                lastIndex = currentIndex;
                fflush(f); // flush every batch
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        fclose(f);
    }).detach();
}
} // namespace utils

#else // ENABLE_TIMING

namespace utils {

struct TimingRecord {
    uint64_t tReceived;
    uint64_t tDeserialized;
    uint64_t tBuffered;
};

inline void recordTiming(const TimingRecord&) {}
inline void startTimingConsumer(const char* = nullptr) {}

} // namespace utils
#endif