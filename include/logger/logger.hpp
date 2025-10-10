#pragma once

#include "events/events.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

constexpr size_t LOGGER_CAPACITY = 4096; // must be power of 2

enum class LogType : uint8_t { GENERIC, MATCH_RESULT, CANCEL, MODIFY, BYTES };

struct RawLogEvent {
    LogType type;
    std::chrono::steady_clock::time_point ts;
    char component[32];

    // Generic text
    char msg[256];

    // For MatchResult
    MatchResult matchRes{};

    // For Modify
    ModifyEvent modEv{};

    // For cancel
    ClientID clientID{};
    OrderID orderID{};
    bool cancelSuccess{};

    // For bytes
    uint8_t bytes[64];
    uint8_t bytes_len{0};
};

class Logger {
public:
    Logger(const std::string& filename, bool enabled)
        : enabled_(enabled), running_(true), head_(0), tail_(0) {
        if (enabled_) {
            out_.open(filename, std::ios::out | std::ios::app);
            worker_ = std::thread([this] { run(); });
            std::cout << "Logger initialized (async structured): " << filename
                      << std::endl;
        }
    }

    ~Logger() {
        stop();
        if (worker_.joinable()) worker_.join();
    }

    inline void log(const std::string& msg, const std::string& component) noexcept {
        if (!enabled_) return;
        RawLogEvent ev{};
        ev.type = LogType::GENERIC;
        ev.ts = std::chrono::steady_clock::now();
        std::strncpy(ev.component, component.c_str(), sizeof(ev.component) - 1);
        std::strncpy(ev.msg, msg.c_str(), sizeof(ev.msg) - 1);
        push(ev);
    }

    inline void log(const MatchResult& result,
                    const std::string& component = "MATCHING_ENGINE") noexcept {
        if (!enabled_) return;
        RawLogEvent ev{};
        ev.type = LogType::MATCH_RESULT;
        ev.ts = std::chrono::steady_clock::now();
        std::strncpy(ev.component, component.c_str(), sizeof(ev.component) - 1);
        ev.matchRes = result;
        push(ev);
    }

    inline void log(ClientID clientID, OrderID orderID, bool success,
                    const std::string& component = "CLIENT") noexcept {
        if (!enabled_) return;
        RawLogEvent ev{};
        ev.type = LogType::CANCEL;
        ev.ts = std::chrono::steady_clock::now();
        std::strncpy(ev.component, component.c_str(), sizeof(ev.component) - 1);
        ev.clientID = clientID;
        ev.orderID = orderID;
        ev.cancelSuccess = success;
        push(ev);
    }

    inline void log(ModifyEvent& modEv, MatchResult& matchRes,
                    const std::string& component = "MATCHING_ENGINE") noexcept {
        if (!enabled_) return;
        RawLogEvent ev{};
        ev.type = LogType::MODIFY;
        ev.ts = std::chrono::steady_clock::now();
        std::strncpy(ev.component, component.c_str(), sizeof(ev.component) - 1);
        ev.modEv = modEv;
        ev.matchRes = matchRes;
        push(ev);
    }

    inline void logBytes(const std::vector<uint8_t>& bytes, const std::string& msg = "",
                         const std::string& component = "PROTOCOL") noexcept {
        if (!enabled_) return;
        RawLogEvent ev{};
        ev.type = LogType::BYTES;
        ev.ts = std::chrono::steady_clock::now();
        std::strncpy(ev.component, component.c_str(), sizeof(ev.component) - 1);
        std::strncpy(ev.msg, msg.c_str(), sizeof(ev.msg) - 1);
        ev.bytes_len = std::min<size_t>(bytes.size(), sizeof(ev.bytes));
        std::memcpy(ev.bytes, bytes.data(), ev.bytes_len);
        push(ev);
    }

    void stop() noexcept { running_.store(false, std::memory_order_release); }

private:
    inline void push(const RawLogEvent& ev) noexcept {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t next = (head + 1) & (LOGGER_CAPACITY - 1);
        if (next == tail_.load(std::memory_order_acquire)) {
            // Buffer full - drop event to stay non-blocking
            dropped_++;
            return;
        }
        buffer_[head] = ev;
        head_.store(next, std::memory_order_release);
    }

    inline bool pop(RawLogEvent& ev) noexcept {
        size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) return false;
        ev = buffer_[tail];
        tail_.store((tail + 1) & (LOGGER_CAPACITY - 1), std::memory_order_release);
        return true;
    }

    void run() {
        RawLogEvent ev{};
        std::ostringstream oss;

        while (running_.load(std::memory_order_acquire) || tail_.load() != head_.load()) {
            bool any = false;
            while (pop(ev)) {
                any = true;
                oss.str("");
                oss.clear();

                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              ev.ts.time_since_epoch())
                              .count();

                oss << "[" << ms << "] [" << ev.component << "] ";

                switch (ev.type) {
                case LogType::GENERIC:
                    oss << ev.msg;
                    break;

                case LogType::MATCH_RESULT: {
                    const auto& r = ev.matchRes;
                    oss << "MatchResult: OrderID=" << r.orderID << " Timestamp=" << r.ts
                        << " Status=" << static_cast<int>(r.status)
                        << " Trades=" << r.tradeVec.size();

                    for (const auto& t : r.tradeVec) {
                        oss << "| Trade[TradeID=" << t.tradeID
                            << " BuyerOrderID=" << t.buyerOrderID
                            << " SellerOrderID=" << t.sellerOrderID
                            << " BuyerID=" << t.buyerID << " SellerID=" << t.sellerID
                            << " Qty=" << t.qty << " Price=" << t.price
                            << " Ts=" << t.timestamp << "]";
                    }
                    break;
                }

                case LogType::CANCEL:
                    oss << "Order Cancel: ClientID=" << ev.clientID
                        << " OrderID=" << ev.orderID << " Success=" << std::boolalpha
                        << ev.cancelSuccess;
                    break;

                case LogType::MODIFY: {
                    const auto& m = ev.modEv;
                    const auto& mr = ev.matchRes;
                    oss << "ModifyEvent: ClientID=" << m.serverClientID
                        << " OldOrderID=" << m.oldOrderID
                        << " NewOrderID=" << m.newOrderID << " NewQty=" << m.newQty
                        << " NewPrice=" << m.newPrice
                        << " Status=" << statusCodes::toStr(m.status);
                    oss << " | MatchResult: OrderID=" << mr.orderID
                        << " Status=" << statusCodes::toStr(mr.status)
                        << " Trades=" << mr.tradeVec.size();
                    break;
                }

                case LogType::BYTES:
                    oss << ev.msg << " | ";
                    for (size_t i = 0; i < ev.bytes_len; ++i)
                        oss << std::hex << std::setw(2) << std::setfill('0')
                            << static_cast<int>(ev.bytes[i]) << " ";
                    break;
                }

                oss << std::dec << "\n";
                out_ << oss.str();
            }

            if (any)
                out_.flush();
            else
                std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        out_.flush();

        if (dropped_ > 0) {
            std::cerr << "[Logger] Dropped " << dropped_ << " events (buffer full)\n";
        }
    }

private:
    std::ofstream out_;
    const bool enabled_;
    std::atomic<bool> running_;
    std::atomic<size_t> dropped_{0};

    alignas(64) RawLogEvent buffer_[LOGGER_CAPACITY];
    std::atomic<size_t> head_;
    std::atomic<size_t> tail_;

    std::thread worker_;
};
