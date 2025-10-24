#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <memory>
#include <thread>

#include "events/eventBus.hpp"

template <typename EventT, size_t MAX_EVENTS = 16384> class GenericEventLogger {
public:
    GenericEventLogger(std::shared_ptr<EventBus> evBus, const std::string& filename)
        : evBus_(evBus), filename_(filename), stop_(false) {
        buffer_.fill({});
        evBus_->subscribe<EventT>([this](const ServerEvent<EventT>& ev) { record(ev); });
        consumerThread_ = std::thread(&GenericEventLogger::startConsumer, this);
    }

    ~GenericEventLogger() {
        stop_ = true;
        if (consumerThread_.joinable()) consumerThread_.join();
    }

private:
    std::shared_ptr<EventBus> evBus_;
    std::string filename_;

    std::array<ServerEvent<EventT>, MAX_EVENTS> buffer_;
    std::atomic<size_t> index_{0};
    std::atomic<bool> stop_;

    std::thread consumerThread_;

    std::atomic<size_t> dropped_{0};
    std::atomic<size_t> lastFlushed_{0};

    void record(const ServerEvent<EventT>& ev) {
        size_t i = index_.fetch_add(1, std::memory_order_relaxed);

        size_t lastFlushedIndex = lastFlushed_.load(std::memory_order_acquire);
        if (i >= lastFlushedIndex + MAX_EVENTS) {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            return; // Don't write
        }

        size_t pos = i % MAX_EVENTS;
        buffer_[pos] = ev;
    }

    void startConsumer() {
        FILE* f = fopen(filename_.c_str(), "w");
        if (!f) return;

        setvbuf(f, nullptr, _IOFBF, 1 << 20);

        fprintf(f, "ts");
        EventT dummy{};
        dummy.iterateElements([&](const char* name, auto&) { fprintf(f, ",%s", name); });
        fprintf(f, "\n");

        size_t lastIndex = 0;

        while (!stop_) {
            size_t currentIndex = index_.load(std::memory_order_acquire);

            if (currentIndex > lastIndex) {
                for (size_t i = lastIndex; i < currentIndex; ++i) {
                    const auto& rec = buffer_[i % MAX_EVENTS];
                    fprintf(f, "%" PRIu64, rec.ts);
                    rec.event.iterateElements([&](const char*, auto& field) {
                        fprintf(f, ",%" PRIu64, uint64_t(field));
                    });
                    fprintf(f, "\n");
                }

                lastIndex = currentIndex;

                lastFlushed_.store(currentIndex, std::memory_order_release);

                fflush(f);
            }

            size_t lost = dropped_.exchange(0, std::memory_order_relaxed);
            if (lost > 0) {
                fprintf(stderr, "[Logger] Dropped %zu events for %s\n", lost,
                        filename_.c_str());
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        fclose(f);
    }
};
