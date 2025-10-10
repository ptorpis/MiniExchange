#pragma once
#include <any>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <thread>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "events/events.hpp"

class EventBus {
public:
    EventBus() = default;
    ~EventBus() { stop(); }

    void start() {
        if (running_.exchange(true)) return; // already running
        worker_ = std::thread([this] { dispatchLoop_(); });
    }

    void stop() {
        {
            std::unique_lock lock(queueMutex_);
            if (!running_) return;

            subscribers_.clear();
            running_ = false;
        }
        queueCv_.notify_all();
        if (worker_.joinable()) worker_.join();
    }

    template <typename EventT>
    void subscribe(std::function<void(const ServerEvent<EventT>&)> handler) {
        std::unique_lock lock(subMutex_);
        auto& vec = subscribers_[std::type_index(typeid(EventT))];
        vec.push_back([handler = std::move(handler)](const std::any& evAny) {
            handler(std::any_cast<const ServerEvent<EventT>&>(evAny));
        });
    }

    template <typename EventT> void publish(const ServerEvent<EventT>& ev) {
        {
            std::unique_lock lock(queueMutex_);
            queue_.emplace(EventWrapper{std::type_index(typeid(EventT)),
                                        std::make_any<ServerEvent<EventT>>(ev)});
        }
        queueCv_.notify_one(); // wake up dispatcher thread
    }

    void clearSubscribers() {
        std::unique_lock lock(subMutex_);
        subscribers_.clear();
    }

private:
    struct EventWrapper {
        std::type_index type{typeid(void)};
        std::any data{};
    };

    void dispatchLoop_() {
        while (true) {
            EventWrapper ev;
            {
                std::unique_lock lock(queueMutex_);
                queueCv_.wait(lock, [&] { return !queue_.empty() || !running_; });

                if (!running_ && queue_.empty()) break;

                ev = std::move(queue_.front());
                queue_.pop();
            }

            std::shared_lock lock(subMutex_);
            auto it = subscribers_.find(ev.type);
            if (it != subscribers_.end()) {
                for (auto& sub : it->second) {
                    sub(ev.data);
                }
            }
        }
    }

    std::unordered_map<std::type_index, std::vector<std::function<void(const std::any&)>>>
        subscribers_;
    mutable std::shared_mutex subMutex_;

    std::queue<EventWrapper> queue_;
    std::mutex queueMutex_;
    std::condition_variable queueCv_;

    std::thread worker_;
    std::atomic<bool> running_;
};
