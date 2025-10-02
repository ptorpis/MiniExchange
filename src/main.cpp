#include "client/client.hpp"
#include "logger/logger.hpp"
#include "protocol/client/clientMessageFactory.hpp"
#include "protocol/messages.hpp"
#include "protocol/protocolHandler.hpp"
#include "protocol/traits.hpp"
#include "server/server.hpp"

#include <iostream>
#include <memory>

int main() {
    std::shared_ptr<Logger> logger = std::make_shared<Logger>("exchange.log");

    logger->log("SERVER", "STARTUP");

    SessionManager sessionManager(logger);
    ProtocolHandler handler(sessionManager, logger);
    uint16_t port = 12345;

    Server server(port, sessionManager, handler, logger);
    if (!server.start(port)) {
        return 1;
    }
    server.run();
    return 0;
}