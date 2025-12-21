#pragma once

#include "core/matchingEngine.hpp"
#include "protocol/clientMessages.hpp"
#include "sessions/sessionManager.hpp"
#include "utils/types.hpp"

class MiniExchangeAPI {
public:
    MiniExchangeAPI(MatchingEngine& engine, SessionManager& sm)
        : engine_(engine), sessionManager_(sm), instrumentID_(engine.getInstrumentID()) {}

    MatchResult processNewOrder(const client::NewOrderPayload& payload);
    bool cancelOrder(const client::CancelOrderPayload& payload);
    ModifyResult modifyOrder(const client::ModifyOrderPayload& payload);

private:
    [[maybe_unused]] MatchingEngine& engine_;
    [[maybe_unused]] SessionManager& sessionManager_;

    const InstrumentID instrumentID_;

    OrderID orderIDSqn_{0};
    OrderID getNextOrderID_() { return ++orderIDSqn_; }
};
