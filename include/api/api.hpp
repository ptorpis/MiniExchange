#pragma once

#include "core/matchingEngine.hpp"
#include "protocol/clientMessages.hpp"
#include "sessions/sessionManager.hpp"
#include "utils/types.hpp"

class MiniExchangeAPI {
public:
    MiniExchangeAPI(MatchingEngine& engine, SessionManager& sm)
        : engine_(engine), sessionManager_(sm), instrumentID_(engine.getInstrumentID()) {}

    [[nodiscard]] MatchResult processNewOrder(const client::NewOrderPayload& payload);
    [[nodiscard]] bool cancelOrder(const client::CancelOrderPayload& payload);
    [[nodiscard]] ModifyResult modifyOrder(const client::ModifyOrderPayload& payload);

private:
    [[maybe_unused]] MatchingEngine& engine_;
    [[maybe_unused]] SessionManager& sessionManager_;

    const InstrumentID instrumentID_;
};
