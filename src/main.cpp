#include "protocol/protocolHandler.hpp"
#include "server/server.hpp"

#include <iostream>

int main() {
    ProtocolHandler handler;
    SessionManager sessionManager;
    uint16_t port = 12345;

    Server server(port, sessionManager, handler);
    if (!server.start(port)) {
        return 1;
    }
    server.run();
    return 0;
}