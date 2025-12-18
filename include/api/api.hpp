#pragma once

#include "core/matchingEngine.hpp"
#include "sessions/sessionManager.hpp"

class MiniExchangeAPI {
public:
    MiniExchangeAPI(SessionManager& sm)
        : engine_(MatchingEngine()), sessionManager_(sm) {}

private:
    MatchingEngine engine_;
    SessionManager& sessionManager_;
};
