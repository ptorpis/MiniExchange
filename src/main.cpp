#include "client/client.hpp"
#include "protocol/client/clientMessageFactory.hpp"
#include "protocol/messages.hpp"
#include "protocol/protocolHandler.hpp"
#include "protocol/traits.hpp"
#include "server/server.hpp"

#include <iostream>

int main() {
    SessionManager sessionManager;
    ProtocolHandler handler(sessionManager);
    uint16_t port = 12345;

    Server server(port, sessionManager, handler);
    if (!server.start(port)) {
        return 1;
    }
    server.run();
    return 0;
}