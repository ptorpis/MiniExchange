#pragma once

#include <array>
#include <cinttypes>
#include <memory>

#include "events/eventBus.hpp"

template <typename EventT, size_t MAX_EVENTS = 16384> class GenericEventLogger {
public:
    GenericEventLogger(std::shared_ptr<EventBus> evBus, const char* filename)
        : evBus_(evBus), filename_(filename) {
        buffer_.fill({});
        evBus_->subscribe<EventT>([this](const ServerEvent<EventT>& ev) { record(ev); });
        startConsumer();
    }

private:
    std::shared_ptr<EventBus> evBus_;
    const char* filename_;

    std::array<ServerEvent<EventT>, MAX_EVENTS> buffer_;
    std::atomic<size_t> index_{0};

    void record(const ServerEvent<EventT>& ev) {
        size_t i = index_.fetch_add(1, std::memory_order_relaxed);
        if (i < MAX_EVENTS) buffer_[i] = ev;
    }

    void startConsumer() {
        std::thread([this] {
            FILE* f = fopen(filename_, "w");
            if (!f) return;
            setvbuf(f, nullptr, _IOFBF, 1 << 20);

            fprintf(f, "tsNs");
            EventT dummy{};
            dummy.iterateElements(
                [&](const char* name, auto&) { fprintf(f, ",%s", name); });
            fprintf(f, "\n");

            size_t lastIndex = 0;
            while (true) {
                size_t currentIndex = index_.load(std::memory_order_acquire);
                if (currentIndex > lastIndex) {
                    for (size_t i = lastIndex; i < currentIndex; ++i) {
                        const auto& rec = buffer_[i];
                        fprintf(f, "%" PRIu64, rec.tsNs);
                        rec.event.iterateElements([&](const char*, auto& field) {
                            fprintf(f, ",%" PRIu64, uint64_t(field));
                        });
                        fprintf(f, "\n");
                    }
                    lastIndex = currentIndex;
                    fflush(f);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            fclose(f);
        }).detach();
    }
};
