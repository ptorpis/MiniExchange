#include "auth/sessionManager.hpp"
#include "protocol/protocolHandler.hpp"
#include <memory>

class OfflineServer {
    OfflineServer(OfflineSessionManager& sm, ProtocolHandler& handler,
                  std::shared_ptr<EventBus> evBus = nullptr)
        : sessionManager_(sm), protocolHandler_(handler), evBus_(evBus) {}

    void start();
    void step();

private:
    OfflineSessionManager& sessionManager_;
    ProtocolHandler& protocolHandler_;
    std::shared_ptr<EventBus> evBus_;

    static const uint64_t TIME_STEP{10};
};
