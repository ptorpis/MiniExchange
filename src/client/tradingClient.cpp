#include "client/tradingClient.hpp"
#include "client/networkClient.hpp"
#include "protocol/messages.hpp"
#include "protocol/serverMessages.hpp"
#include "utils/status.hpp"
#include "utils/timing.hpp"
#include "utils/types.hpp"
#include "utils/utils.hpp"
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

TradingClient::TradingClient(std::string host, std::uint16_t port)
    : network_(std::move(host), port) {

    // Register callbacks with NetworkClient
    network_.setHelloAckCallback([this](const auto& msg) { handleHelloAck_(msg); });

    network_.setOrderAckCallback([this](const auto& msg) { handleOrderAck_(msg); });

    network_.setCancelAckCallback([this](const auto& msg) { handleCancelAck_(msg); });

    network_.setModifyAckCallback([this](const auto& msg) { handleModifyAck_(msg); });

    network_.setTradeCallback([this](const auto& msg) { handleTrade_(msg); });
}

bool TradingClient::connect() {
    if (!network_.connect()) {
        return false;
    }

    network_.sendHello();
    return true;
}

void TradingClient::disconnect() {
    network_.sendLogout();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    network_.disconnect();
}

void TradingClient::submitOrder(InstrumentID instrumentID, OrderSide side, Qty qty,
                                Price price, OrderType type, TimeInForce tif,
                                Timestamp goodTill) {
    ClientOrderID clientOrderID = network_.getNextClientOrderID();

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

    network_.sendNewOrder(instrumentID, side, type, qty, price, clientOrderID, tif,
                          goodTill);

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
            return false; // Can't cancel pending order (no server ID yet)
        }

        if (!it->second.isOpen()) {
            return false; // Order not in cancelable state
        }

        serverOrderID = it->second.serverOrderID;
        instrumentID = it->second.instrumentID;
    }

    network_.sendCancel(clientOrderID, serverOrderID, instrumentID);

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

    network_.sendModify(clientOrderID, serverOrderID, newQty, newPrice, instrumentID);
}

std::optional<ClientOrder> TradingClient::getOrder(ClientOrderID clientOrderID) const {

    std::lock_guard<std::mutex> lock(stateMutex_);

    auto it = orders_.find(clientOrderID);
    if (it == orders_.end()) {
        return std::nullopt;
    }

    return it->second;
}

std::vector<ClientOrder> TradingClient::getPendingOrders() const {
    std::lock_guard<std::mutex> lock(stateMutex_);

    std::vector<ClientOrder> pending;
    for (const auto& [id, order] : orders_) {
        if (order.isPending()) {
            pending.push_back(order);
        }
    }

    return pending;
}

std::vector<ClientOrder> TradingClient::getOpenOrders() const {
    std::lock_guard<std::mutex> lock(stateMutex_);

    std::vector<ClientOrder> open;
    for (const auto& [id, order] : orders_) {
        if (order.isOpen()) {
            open.push_back(order);
        }
    }

    return open;
}

std::vector<ClientOrder> TradingClient::getAllOrders() const {
    std::lock_guard<std::mutex> lock(stateMutex_);

    std::vector<ClientOrder> all;
    all.reserve(orders_.size());

    for (const auto& [id, order] : orders_) {
        all.push_back(order);
    }

    return all;
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
    std::lock_guard<std::mutex> lock(stateMutex_);
    // need market data to calculate this
    return 0;
}

void TradingClient::handleHelloAck_(
    [[maybe_unused]] const Message<server::HelloAckPayload>& msg) {
    // nothing to do, network client takes care of bookkeeping
}

void TradingClient::handleOrderAck_(const Message<server::OrderAckPayload>& msg) {
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

void TradingClient::handleCancelAck_(const Message<server::CancelAckPayload>& msg) {
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

void TradingClient::handleModifyAck_(const Message<server::ModifyAckPayload>& msg) {
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

void TradingClient::handleTrade_(const Message<server::TradePayload>& msg) {
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

        updatePosition_(instrumentID, side, fillQty, fillPrice);
    }

    if (found) {
        onOrderFilled(clientOrderID, fillPrice, fillQty);
    }
}

void TradingClient::updatePosition_(InstrumentID instrumentID, OrderSide side, Qty qty,
                                    Price price) {
    // Assumes mutex is already held
    auto& position = positions_[instrumentID];

    if (side == OrderSide::BUY) {
        position.longQty = Qty{position.longQty.value() + qty.value()};
    } else {
        position.shortQty = Qty{position.shortQty.value() + qty.value()};
    }

    // TODO: Track average price for P&L calculation
    (void)price;
}
