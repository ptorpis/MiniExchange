#pragma once

#include "client/mdReceiver.hpp"
#include "client/networkClient.hpp"
#include "utils/types.hpp"
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

struct TradingConfig {
    std::string host = "127.0.0.1";
    std::uint16_t port = 12345;
    MDConfig mdConfig;
    bool enabledMarketData = true;
};

struct Position {
    Qty longQty{0};
    Qty shortQty{0};

    std::int64_t netPosition() const {
        return static_cast<std::int64_t>(longQty.value()) -
               static_cast<std::int64_t>(shortQty.value());
    }

    bool isFlat() const { return netPosition() == 0; }
};

class TradingClient {
public:
    explicit TradingClient(const TradingConfig& config);

    virtual ~TradingClient() = default;

    bool connect();
    void disconnect();
    bool isConnected() const { return network_.isConnected(); }

    void submitOrder(InstrumentID instrumentID, OrderSide side, Qty qty, Price price,
                     OrderType type = OrderType::LIMIT,
                     TimeInForce tif = TimeInForce::GOOD_TILL_CANCELLED,
                     Timestamp goodTill = Timestamp{0});

    bool cancelOrder(ClientOrderID clientOrderID);
    void modifyOrder(ClientOrderID clientOrderID, Qty newQty, Price newPrice);

    std::optional<ClientOrder> getOrder(ClientOrderID clientOrderID) const;
    std::vector<ClientOrder> getPendingOrders() const;
    std::vector<ClientOrder> getOpenOrders() const;
    std::vector<ClientOrder> getAllOrders() const;
    Position getPosition(InstrumentID instrumentID) const;
    std::int64_t getUnrealizedPnL() const;

    const Level2OrderBook& getOrderBook() const {
        if (network_.isMarketDataEnabled()) {
            return network_.getMarketData()->getOrderBook();
        }
        static Level2OrderBook emptyBook;
        return emptyBook;
    }

    bool isBookValid() const {
        return network_.getMarketData() && network_.getMarketData()->isBookValid();
    }

protected:
    virtual void onOrderSubmitted([[maybe_unused]] ClientOrderID clientOrderID) {}
    virtual void onOrderAccepted([[maybe_unused]] ClientOrderID clientOrderID,
                                 [[maybe_unused]] OrderID serverOrderID,
                                 [[maybe_unused]] Price acceptedPrice) {}
    virtual void onOrderRejected([[maybe_unused]] ClientOrderID clientOrderID,
                                 [[maybe_unused]] OrderStatus status) {}
    virtual void onOrderFilled([[maybe_unused]] ClientOrderID clientOrderID,
                               [[maybe_unused]] Price fillPrice,
                               [[maybe_unused]] Qty fillQty) {}
    virtual void onOrderCancelled([[maybe_unused]] ClientOrderID clientOrderID) {}
    virtual void onCancelRejected([[maybe_unused]] ClientOrderID clientOrderID) {}
    virtual void onModifyAccepted([[maybe_unused]] ClientOrderID clientOrderID,
                                  [[maybe_unused]] OrderID newServerOrderID,
                                  [[maybe_unused]] Qty newQty,
                                  [[maybe_unused]] Price newPrice) {}
    virtual void onModifyRejected([[maybe_unused]] ClientOrderID clientOrderID) {}

    virtual void onBookSnapshot([[maybe_unused]] const Level2OrderBook& book,
                                [[maybe_unused]] std::uint64_t seqNum) {}
    virtual void onBookDelta([[maybe_unused]] Price price, [[maybe_unused]] Qty qty,
                             [[maybe_unused]] OrderSide side,
                             [[maybe_unused]] MDDeltatype type,
                             [[maybe_unused]] std::uint64_t seqNum) {}
    virtual void onBookValid() {}
    virtual void onBookInvalid() {}
    virtual void onGapDetected([[maybe_unused]] std::uint64_t expected,
                               [[maybe_unused]] std::uint64_t received) {}

private:
    NetworkClient network_;

    mutable std::mutex stateMutex_;
    std::unordered_map<ClientOrderID, ClientOrder> orders_;
    std::unordered_map<OrderID, ClientOrderID> serverToClientID_;
    std::unordered_map<InstrumentID, Position> positions_;

    void handleHelloAck_(const Message<server::HelloAckPayload>& msg);
    void handleOrderAck_(const Message<server::OrderAckPayload>& msg);
    void handleCancelAck_(const Message<server::CancelAckPayload>& msg);
    void handleModifyAck_(const Message<server::ModifyAckPayload>& msg);
    void handleTrade_(const Message<server::TradePayload>& msg);

    void updatePosition_(InstrumentID instrumentID, OrderSide side, Qty qty, Price price);

    void setupMarketDataCallbacks_();
};
