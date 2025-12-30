#pragma once

#include "protocol/messages.hpp"
#include "protocol/serverMessages.hpp"
#include "sessions/clientSession.hpp"
#include "utils/types.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

class NetworkClient {
public:
    NetworkClient(std::string host, std::uint16_t port);
    virtual ~NetworkClient();

    bool connect();
    bool isConnected() const { return session_.isConnected(); }
    void disconnect();

    void sendHello();
    void sendLogout();

    void sendNewOrder(InstrumentID instrumentID, OrderSide side, OrderType type, Qty qty,
                      Price price, ClientOrderID clientOrderID,
                      TimeInForce timeInForce = TimeInForce::GOOD_TILL_CANCELLED,
                      Timestamp goodTillDate = Timestamp{0});

    void sendCancel(ClientOrderID clientOrderID, OrderID serverOrderID,
                    InstrumentID instrumentID);

    void sendModify(ClientOrderID clientOrderID, OrderID serverOrderID, Qty newQty,
                    Price newPrice, InstrumentID instrumentID);

    using HelloAckCallback = std::function<void(const Message<server::HelloAckPayload>&)>;
    using LogoutAckCallback =
        std::function<void(const Message<server::LogoutAckPayload>&)>;
    using OrderAckCallback = std::function<void(const Message<server::OrderAckPayload>&)>;
    using CancelAckCallback =
        std::function<void(const Message<server::CancelAckPayload>&)>;
    using ModifyAckCallback =
        std::function<void(const Message<server::ModifyAckPayload>&)>;
    using TradeCallback = std::function<void(const Message<server::TradePayload>&)>;

    void setHelloAckCallback(HelloAckCallback cb) { helloAckCallback_ = std::move(cb); }
    void setLogoutAckCallback(LogoutAckCallback cb) {
        logoutAckCallback_ = std::move(cb);
    }
    void setOrderAckCallback(OrderAckCallback cb) { orderAckCallback_ = std::move(cb); }
    void setCancelAckCallback(CancelAckCallback cb) {
        cancelAckCallback_ = std::move(cb);
    }
    void setModifyAckCallback(ModifyAckCallback cb) {
        modifyAckCallback_ = std::move(cb);
    }
    void setTradeCallback(TradeCallback cb) { tradeCallback_ = std::move(cb); }

    ClientOrderID getNextClientOrderID() { return session_.getNextOrderID(); }

private:
    void messageLoop_();
    void startMessageLoop_();
    void stopMessageLoop_();

    template <typename Payload>
    void sendMessage_(MessageType type, const Payload& payload);

    void processRecvBuffer_();
    void handleMessage_(std::span<const std::byte> messageBytes);
    void setNonBlocking_();
    void setTCPNoDelay_();

    ClientSession session_;

    std::atomic<bool> running_{false};
    std::thread messageThread_;

    HelloAckCallback helloAckCallback_;
    LogoutAckCallback logoutAckCallback_;
    OrderAckCallback orderAckCallback_;
    CancelAckCallback cancelAckCallback_;
    ModifyAckCallback modifyAckCallback_;
    TradeCallback tradeCallback_;
};
