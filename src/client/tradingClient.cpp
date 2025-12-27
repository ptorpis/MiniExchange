#include "client/tradingClient.hpp"
#include "client/networkClient.hpp"
#include "protocol/serverMessages.hpp"
#include "utils/status.hpp"
#include "utils/timing.hpp"
#include "utils/types.hpp"
#include "utils/utils.hpp"
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

TradingClient::TradingClient(std::string host, std::uint16_t port)
    : NetworkClient(std::move(host), port) {}

void TradingClient::submitOrder(InstrumentID instrumentID, OrderSide side, Qty qty,
                                Price price, OrderType type, TimeInForce tif,
                                Timestamp goodTill) {
    ClientOrderID clientOrderID = getNextClientOrderID();

    {
        std::lock_guard<std::mutex> lock(stateMutex_);

        ClientOrder order{.orderID = clientOrderID,
                          .serverOrderID = OrderID{0},
                          .instrumentID = instrumentID,
                          .side = side,
                          .type = type,
                          .price = price,
                          .originalQty = qty,
                          .remainingQty = qty,
                          .status = OrderStatus::PENDING,
                          .tif = tif,
                          .goodTillDate = goodTill,
                          .submitTime = TSCClock::now()};

        orders_[clientOrderID] = order;
    }

    sendNewOrder(instrumentID, side, type, qty, price, TimeInForce::GOOD_TILL_CANCELLED,
                 0);
    onOrderSubmitted(clientOrderID);
}

bool TradingClient::cancelOrder(ClientOrderID clientOrderID) {
    OrderID serverOrderID;
    InstrumentID instrumentID;

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        auto it = orders_.find(clientOrderID);
        if (it == orders_.end()) {
            return false;
        }

        if (it->second.isPending()) {
            return false;
        }

        if (!it->second.isOpen()) {
            return false;
        }

        serverOrderID = it->second.serverOrderID;
        instrumentID = it->second.instrumentID;
    }

    sendCancel(clientOrderID, serverOrderID, instrumentID);
    return true;
}

void TradingClient::modifyOrder(ClientOrderID clientOrderID, Qty newQty, Price newPrice) {
    OrderID serverOrderID;
    InstrumentID instrumentID;

    {
        std::lock_guard<std::mutex> lock(stateMutex_);

        auto it = orders_.find(clientOrderID);
        if (it == orders_.end()) {
            return;
        }

        if (it->second.isPending()) {
            return;
        }

        if (!it->second.isOpen()) {
            return;
        }

        serverOrderID = it->second.serverOrderID;
        instrumentID = it->second.instrumentID;
    }

    sendModify(clientOrderID, serverOrderID, newQty, newPrice, instrumentID);
}

std::optional<ClientOrder> TradingClient::getOrder(ClientOrderID clientOrderID) const {

    std::lock_guard<std::mutex> lock(stateMutex_);

    auto it = orders_.find(clientOrderID);
    if (it == orders_.end()) {
        return std::nullopt;
    }

    return it->second;
}

std::vector<ClientOrderID> TradingClient::getPendingOrders() const {
    std::lock_guard<std::mutex> lock(stateMutex_);

    std::vector<ClientOrderID> pending;
    for (const auto& [id, order] : orders_) {
        if (order.isPending()) {
            pending.push_back(id);
        }
    }

    return pending;
}

std::vector<ClientOrderID> TradingClient::getOpenOrders() const {
    std::lock_guard<std::mutex> lock(stateMutex_);

    std::vector<ClientOrderID> openOrders;
    for (const auto& [id, order] : orders_) {
        if (order.isOpen()) {
            openOrders.push_back(id);
        }
    }

    return openOrders;
}

std::vector<ClientOrderID> TradingClient::getAllOrders() const {
    std::lock_guard<std::mutex> lock(stateMutex_);

    std::vector<ClientOrderID> orders;
    for (const auto& [id, order] : orders_) {
        orders.push_back(id);
    }
    return orders;
}

Position TradingClient::getPosition(InstrumentID instrumentID) const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    auto it = positions_.find(instrumentID);
    if (it == positions_.end()) {
        return Position{};
    }

    return it->second;
}

std::int64_t TradingClient::getUnrealizedPnL() const {
    // need market data to calculate this
    return 0;
}

void TradingClient::onOrderAck(const Message<server::OrderAckPayload>& msg) {
    utils::printMessage(std::cout, msg);
    ClientOrderID clientOrderID{msg.payload.clientOrderID};
    OrderID serverOrderID{msg.payload.serverOrderID};

    bool found = false;

    {
        std::lock_guard<std::mutex> lock(stateMutex_);

        auto it = orders_.find(clientOrderID);
        if (it == orders_.end()) {
            return;
        }

        found = true;

        auto& order = it->second;

        if (msg.payload.status == +OrderStatus::NEW ||
            msg.payload.status == +OrderStatus::PARTIALLY_FILLED) {
            order.serverOrderID = OrderID{msg.payload.serverOrderID};
            order.status = OrderStatus{msg.payload.status};
            order.remainingQty = Qty{msg.payload.remainingQty};
            order.price = Price{msg.payload.acceptedPrice};

            serverToClientID_[serverOrderID] = clientOrderID;

        } else {
            order.status = OrderStatus{msg.payload.status};
        }
    }

    if (found) {
        if (msg.payload.status == +OrderStatus::NEW ||
            msg.payload.status == +OrderStatus::PARTIALLY_FILLED ||
            msg.payload.status == +OrderStatus::FILLED) {
            onOrderAccepted(clientOrderID, serverOrderID,
                            Price{msg.payload.acceptedPrice});
        } else {
            onOrderRejected(clientOrderID, OrderStatus{msg.payload.status});
        }
    }
}

void TradingClient::onCancelAck(const Message<server::CancelAckPayload>& msg) {
    ClientOrderID clientOrderID{msg.payload.clientOrderID};
    status::CancelStatus cancelStatus = status::CancelStatus{msg.payload.status};

    bool found{false};

    {
        std::lock_guard<std::mutex> lock(stateMutex_);

        auto it = orders_.find(clientOrderID);
        if (it == orders_.end()) {
            return;
        }

        found = true;

        if (cancelStatus == status::CancelStatus::ACCEPTED) {
            auto& order = it->second;
            order.status = OrderStatus::CANCELLED;
            order.remainingQty = Qty{0};
        }
    }

    if (found) {
        onOrderCancelled(clientOrderID);
    }
}

void TradingClient::onModifyAck(const Message<server::ModifyAckPayload>& msg) {
    bool found{false};
    {
        std::lock_guard<std::mutex> lock(stateMutex_);

        auto it = orders_.find(ClientOrderID{msg.payload.clientOrderID});
        if (it == orders_.end()) {
            return;
        }

        found = true;

        if (ModifyStatus{msg.payload.status} == ModifyStatus::ACCEPTED) {
            auto& order = it->second;
            serverToClientID_.erase(OrderID{msg.payload.oldServerOrderID});
            serverToClientID_.insert({OrderID{msg.payload.newServerOrderID},
                                      ClientOrderID{msg.payload.clientOrderID}});

            order.serverOrderID = OrderID{msg.payload.newServerOrderID};
            order.price = Price{msg.payload.newPrice};
            order.originalQty = Qty{msg.payload.newQty};
            order.remainingQty = Qty{msg.payload.newQty};
            order.status = OrderStatus::MODIFIED;
        }
    }

    if (found) {
        if (msg.payload.status == +ModifyStatus::ACCEPTED) {
            onModifyAccepted(ClientOrderID{msg.payload.clientOrderID},
                             OrderID{msg.payload.newServerOrderID},
                             Qty{msg.payload.newQty}, Price{msg.payload.newPrice});
        } else {
            onModifyRejected(ClientOrderID{msg.payload.clientOrderID});
        }
    }
}

void TradingClient::onTrade(const Message<server::TradePayload>& msg) {
    ClientOrderID clientOrderID{msg.payload.clientOrderID};
    Qty fillQty{msg.payload.filledQty};
    Price fillPrice{msg.payload.filledPrice};

    InstrumentID instrumentID;
    OrderSide side;
    bool found = false;

    {
        std::lock_guard<std::mutex> lock(stateMutex_);

        auto it = orders_.find(clientOrderID);
        if (it == orders_.end()) {
            return;
        }

        found = true;
        auto& order = it->second;

        instrumentID = order.instrumentID;
        side = order.side;

        Qty newRemaining{order.remainingQty.value() - fillQty.value()};
        order.remainingQty = newRemaining;

        if (newRemaining.value() == 0) {
            order.status = OrderStatus::FILLED;
        } else {
            order.status = OrderStatus::PARTIALLY_FILLED;
        }

        updatePosition_(instrumentID, side, fillQty);
    }

    if (found) {
        onOrderFilled(clientOrderID, fillPrice, fillQty);
    }
}

void TradingClient::updateOrderState_(ClientOrderID clientOrderID, OrderStatus status,
                                      Qty qty) {
    // Assumes mutex is already held
    auto it = orders_.find(clientOrderID);
    if (it != orders_.end()) {
        it->second.status = status;
        it->second.remainingQty = qty;
    }
}

void TradingClient::updatePosition_(InstrumentID instrumentID, OrderSide side, Qty qty) {
    // Assumes mutex is already held
    auto& position = positions_[instrumentID];

    if (side == OrderSide::BUY) {
        position.longQty = Qty{position.longQty.value() + qty.value()};
    } else {
        position.shortQty = Qty{position.shortQty.value() + qty.value()};
    }
}
