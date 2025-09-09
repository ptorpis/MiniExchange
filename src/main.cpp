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
    // const std::vector<uint8_t> clientSendBuffer = client.readSendBuffer();
    std::cout << "Data sent from the client to the Server" << std::endl;
    utils::printHex(client.readSendBuffer());

    Session& serverSession = sessionManager.createSession(serverFD);
    serverSession.hmacKey = HMACKey;
    serverSession.recvBuffer.insert(std::end(serverSession.recvBuffer),
                                    std::begin(client.readSendBuffer()),
                                    std::end(client.readSendBuffer()));

    handler.onMessage(serverFD);

    // const std::vector<uint8_t> serverSendBuffer = serverSession.sendBuffer;
    std::cout << "Server's HELLO ACK response" << std::endl;
    utils::printHex(serverSession.sendBuffer);

    client.clearSendBuffer();
    client.appendRecvBuffer(serverSession.sendBuffer);
    client.processIncoming();

    serverSession.sendBuffer.clear();

    std::cout << "Server side authenticated " << std::boolalpha
              << serverSession.authenticated << std::endl;
    std::cout << "Client side authenticated " << std::boolalpha << client.getAuthStatus()
              << std::endl;

    // test fill

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

    client.testFill();
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

    // cancel

    client.sendTestOrder();

    serverSession.recvBuffer.insert(std::end(serverSession.recvBuffer),
                                    std::begin(client.readSendBuffer()),
                                    std::end(client.readSendBuffer()));
    handler.onMessage(serverFD);
    client.clearSendBuffer();
    client.appendRecvBuffer(serverSession.sendBuffer);
    client.processIncoming();

    serverSession.sendBuffer.clear();

    OrderID orderID = 3;
    client.sendCancel(orderID);
    serverSession.recvBuffer.insert(std::end(serverSession.recvBuffer),
                                    std::begin(client.readSendBuffer()),
                                    std::end(client.readSendBuffer()));
    handler.onMessage(serverFD);
    client.clearSendBuffer();
    client.appendRecvBuffer(serverSession.sendBuffer);
    client.processIncoming();

    serverSession.sendBuffer.clear();
    //

    // test modify order
    client.sendTestOrder();
    serverSession.recvBuffer.insert(std::end(serverSession.recvBuffer),
                                    std::begin(client.readSendBuffer()),
                                    std::end(client.readSendBuffer()));
    handler.onMessage(serverFD);
    client.clearSendBuffer();
    client.appendRecvBuffer(serverSession.sendBuffer);
    client.processIncoming();

    serverSession.sendBuffer.clear();

    OrderID oldID = 4;

    client.sendModify(oldID, 90, 300);
    serverSession.recvBuffer.insert(std::end(serverSession.recvBuffer),
                                    std::begin(client.readSendBuffer()),
                                    std::end(client.readSendBuffer()));
    handler.onMessage(serverFD);
    client.clearSendBuffer();
    client.appendRecvBuffer(serverSession.sendBuffer);
    client.processIncoming();
    serverSession.sendBuffer.clear();

    //

    // logout
    client.sendLogout();
    std::cout << "Client's logout message" << std::endl;
    utils::printHex(client.readSendBuffer());

    serverSession.recvBuffer.insert(std::end(serverSession.recvBuffer),
                                    std::begin(client.readSendBuffer()),
                                    std::end(client.readSendBuffer()));

    handler.onMessage(serverFD);
    std::cout << "Server's logout ack" << std::endl;

    client.clearSendBuffer();
    client.appendRecvBuffer(serverSession.sendBuffer);
    client.processIncoming();

    utils::printHex(serverSession.sendBuffer);

    std::cout << "Server side authenticated " << std::boolalpha
              << serverSession.authenticated << std::endl;
    std::cout << "Client side authenticated " << std::boolalpha << client.getAuthStatus()
              << std::endl;

    return 0;
}