#pragma once

#include "client/networkClient.hpp"
#include "core/order.hpp"
#include "protocol/serverMessages.hpp"
#include "utils/types.hpp"
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

struct Position {
    Qty longQty{0};
    Qty shortQty{0};

    std::int64_t netPosition() const {
        return static_cast<std::int64_t>(longQty.value()) -
               static_cast<std::int64_t>(shortQty.value());
    }

    bool isFlat() const { return netPosition() == 0; }
};

class TradingClient : public NetworkClient {
public:
    TradingClient(std::string host, std::uint16_t port);
    virtual ~TradingClient() = default;
    void submitOrder(InstrumentID instrumentID, OrderSide side, Qty qty, Price price,
                     OrderType type, TimeInForce tif, Timestamp goodTill);
    void submitOrder(ClientOrder order);
    bool cancelOrder(ClientOrderID orderID);
    void modifyOrder(ClientOrderID orderID, Qty newQty, Price newPrice);

    std::optional<ClientOrder> getOrder(ClientOrderID orderID) const;
    std::vector<ClientOrderID> getPendingOrders() const;
    std::vector<ClientOrderID> getOpenOrders() const;
    std::vector<ClientOrderID> getAllOrders() const;
    Position getPosition(InstrumentID instrumentID) const;

    std::int64_t getUnrealizedPnL() const;

protected:
    virtual void onOrderSubmitted([[maybe_unused]] ClientOrderID clientOrderID) {}

    virtual void onOrderAccepted([[maybe_unused]] ClientOrderID clientOrderID,
                                 [[maybe_unused]] OrderID serverOrderID,
                                 [[maybe_unused]] Price acceptedPrice) {}

    virtual void onOrderRejected([[maybe_unused]] ClientOrderID clientOrderID,
                                 [[maybe_unused]] OrderStatus reason) {}

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

    void onOrderAck(const Message<server::OrderAckPayload>& msg) override;
    void onCancelAck(const Message<server::CancelAckPayload>& msg) override;
    void onModifyAck(const Message<server::ModifyAckPayload>& msg) override;
    void onTrade(const Message<server::TradePayload>& msg) override;

private:
    void updateOrderState_(ClientOrderID orderID, OrderStatus status, Qty qty);
    void updatePosition_(InstrumentID instrumentID, OrderSide side, Qty qty);

    mutable std::mutex stateMutex_;

    std::unordered_map<ClientOrderID, ClientOrder> orders_;
    std::unordered_map<OrderID, ClientOrderID> serverToClientID_;
    std::unordered_map<InstrumentID, Position> positions_;
};
