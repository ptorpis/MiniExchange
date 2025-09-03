#include <algorithm>
#include <iostream>

#include "api/api.hpp"
#include "auth/sessionManager.hpp"
#include "client/client.hpp"
#include "core/matchingEngine.hpp"
#include "core/order.hpp"
#include "network/networkHandler.hpp"
#include "utils/types.hpp"
#include "utils/utils.hpp"

int main() {

    int clientFD = 1;
    int serverFD = 2;

    MatchingEngine engine;
    SessionManager sessionManager;
    OrderService service;
    MiniExchangeAPI api = MiniExchangeAPI(engine, sessionManager, service);
    NetworkHandler handler = NetworkHandler(api, sessionManager);

    std::array<uint8_t, 32> HMACKey;
    std::fill(HMACKey.begin(), HMACKey.end(), 0x11);
    std::array<uint8_t, 16> APIKEY;
    std::fill(APIKEY.begin(), APIKEY.end(), 0x22);

    Client client = Client(HMACKey, APIKEY, clientFD);

    client.sendHello();
    const std::vector<uint8_t> clientSendBuffer = client.readSendBuffer();
    std::cout << "Data sent from the client to the Server" << std::endl;
    utils::printHex(clientSendBuffer);

    Session& serverSession = sessionManager.createSession(serverFD);
    serverSession.hmacKey = HMACKey;
    serverSession.recvBuffer.insert(std::end(serverSession.recvBuffer),
                                    std::begin(clientSendBuffer),
                                    std::end(clientSendBuffer));

    handler.onMessage(serverFD);

    const std::vector<uint8_t> serverSendBuffer = serverSession.sendBuffer;
    std::cout << "Server's HELLO ACK response" << std::endl;
    utils::printHex(serverSendBuffer);

    client.clearSendBuffer();
    client.appendRecvBuffer(serverSendBuffer);
    client.processIncoming();

    serverSession.sendBuffer.clear();

    std::cout << "Server side authenticated " << std::boolalpha
              << serverSession.authenticated << std::endl;
    std::cout << "Client side authenticated " << std::boolalpha << client.getAuthStatus()
              << std::endl;

    //

    client.sendTestOrder();
    serverSession.recvBuffer.insert(std::end(serverSession.recvBuffer),
                                    std::begin(client.readSendBuffer()),
                                    std::end(client.readSendBuffer()));

    handler.onMessage(serverFD);
    client.clearSendBuffer();
    client.appendRecvBuffer(serverSession.sendBuffer);
    client.processIncoming();

    client.clearSendBuffer();
    serverSession.sendBuffer.clear();
    //

    client.sendLogout();
    const std::vector<uint8_t> clientSendBufferLogout = client.readSendBuffer();
    std::cout << "Client's logout message" << std::endl;
    utils::printHex(clientSendBufferLogout);

    serverSession.recvBuffer.insert(std::end(serverSession.recvBuffer),
                                    std::begin(clientSendBufferLogout),
                                    std::end(clientSendBufferLogout));

    handler.onMessage(serverFD);
    const std::vector<uint8_t> serverSendBufferLogout = serverSession.sendBuffer;
    std::cout << "Server's logout ack" << std::endl;

    client.clearSendBuffer();
    client.appendRecvBuffer(serverSendBufferLogout);
    client.processIncoming();

    utils::printHex(serverSendBufferLogout);

    std::cout << "Server side authenticated " << std::boolalpha
              << serverSession.authenticated << std::endl;
    std::cout << "Client side authenticated " << std::boolalpha << client.getAuthStatus()
              << std::endl;

    return 0;
}