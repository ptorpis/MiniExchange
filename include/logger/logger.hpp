#pragma once

#include "core/trade.hpp"

#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <queue>
#include <string>
#include <thread>
#include <vector>

const bool LOGGING_ENABLED{true};

struct LogEntry {
    std::string component;
    std::string message;
    std::vector<uint8_t> bytes;
    std::chrono::steady_clock::time_point timestamp;
};

template <bool EnabledFlag = LOGGING_ENABLED> class Logger {
public:
    Logger(const std::string& filename, size_t batchSize = 64)
        : outFile_(filename, std::ios::out | std::ios::app), running_(true),
          batchSize_(batchSize) {
        if constexpr (EnabledFlag) {
            worker_ = std::thread([this]() { this->run(); });
            std::cout << "Logger initialized, logging to " << filename << std::endl;
        }
    }

    ~Logger() {
        stop();
        if (worker_.joinable()) worker_.join();
    }

    void log(const std::string& msg, const std::string& component) {
        if constexpr (EnabledFlag) {
            buffer_.push({component, msg, {}, std::chrono::steady_clock::now()});
        }
    }

    void log(const MatchResult& result,
             const std::string& component = "MATCHING_ENGINE") {
        if constexpr (EnabledFlag) {
            std::ostringstream oss;
            oss << "MatchResult: " << "OrderID=" << result.orderID
                << " Timestamp=" << result.ts
                << " Status=" << static_cast<int>(result.status)
                << " Trades=" << result.tradeVec.size();

            for (const auto& t : result.tradeVec) {
                oss << "\n  Trade[" << "TradeID=" << t.tradeID
                    << " BuyerOrderID=" << t.buyerOrderID
                    << " SellerOrderID=" << t.sellerOrderID << " BuyerID=" << t.buyerID
                    << " SellerID=" << t.sellerID << " Qty=" << t.qty
                    << " Price=" << t.price << " Ts=" << t.timestamp << "]";
            }

            buffer_.push({component, oss.str(), {}, std::chrono::steady_clock::now()});
        }
    }

    // order removals
    void log(ClientID clientID, OrderID orderID, bool success,
             const std::string& component) {
        if constexpr (EnabledFlag) {
            std::ostringstream oss;
            oss << "Order Cancel: ClientID=" << clientID << " OrderID=" << orderID
                << std::boolalpha << " Success=" << success;

            buffer_.push({component, oss.str(), {}, std::chrono::steady_clock::now()});
        }
    }

    void log(ModifyEvent& modEv, MatchResult& matchRes, const std::string& component) {
        std::ostringstream oss;

        if constexpr (EnabledFlag) {
            oss << "ModifyEvent: " << "ClientID=" << modEv.serverClientID
                << " OldOrderID=" << modEv.oldOrderID
                << " NewOrderID= " << modEv.newOrderID
                << " status=" << statusCodes::toStr(modEv.status);

            oss << "MatchResult: " << "OrderID=" << matchRes.orderID
                << " Timestamp=" << matchRes.ts
                << " Status=" << statusCodes::toStr(matchRes.status)
                << " Trades=" << matchRes.tradeVec.size();

            for (const auto& t : matchRes.tradeVec) {
                oss << "\n  Trade[" << "TradeID=" << t.tradeID
                    << " BuyerOrderID=" << t.buyerOrderID
                    << " SellerOrderID=" << t.sellerOrderID << " BuyerID=" << t.buyerID
                    << " SellerID=" << t.sellerID << " Qty=" << t.qty
                    << " Price=" << t.price << " Ts=" << t.timestamp << "]";
            }
            buffer_.push({component, oss.str(), {}, std::chrono::steady_clock::now()});
        }
    }

    void logBytes(const std::vector<uint8_t>& bytes, const std::string& msg = "",
                  const std::string& component = "PROTOCOL") {
        if constexpr (EnabledFlag) {
            buffer_.push({component, msg, bytes, std::chrono::steady_clock::now()});
        }
    }

    void stop() { running_ = false; }

private:
    void run() {
        std::vector<LogEntry> batch;
        batch.reserve(batchSize_);

        while (running_ || !buffer_.empty()) {
            while (!buffer_.empty() && batch.size() < batchSize_) {
                batch.push_back(buffer_.front());
                buffer_.pop();
            }

            for (const auto& entry : batch) {
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              entry.timestamp.time_since_epoch())
                              .count();
                outFile_ << "[" << std::dec << ms << "] [" << entry.component << "] "
                         << entry.message;
                if (!entry.bytes.empty()) {
                    outFile_ << " | ";
                    for (auto b : entry.bytes) {
                        outFile_ << std::hex << std::setw(2) << std::setfill('0')
                                 << static_cast<int>(b) << " ";
                    }
                }
                outFile_ << "\n";
            }

            if (!batch.empty()) {
                outFile_.flush();
                batch.clear();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    std::ofstream outFile_;
    std::atomic<bool> running_;
    size_t batchSize_;
    std::queue<LogEntry> buffer_;
    std::thread worker_;
};
