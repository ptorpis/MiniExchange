#include "client/client.hpp"
#include "protocol/messages.hpp"

#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>

int main() {
    const char* serverIP = "127.0.0.1";
    int serverPort = 12345;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket failed");
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);

    if (inet_pton(AF_INET, serverIP, &serverAddr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return 1;
    }

    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("connection failed");
        close(sock);
        return 1;
    }

    std::cout << "Connected to MiniExchange at " << serverIP << ":" << serverPort
              << std::endl;

    std::array<uint8_t, 32> HMACKEY{};
    std::array<uint8_t, 16> APIKEY{};

    HMACKEY.fill(0x11);
    APIKEY.fill(0x22);

    Client client(HMACKEY, APIKEY, sock, [&](const std::span<const uint8_t> buffer) {
        std::cout << "Client sending data:" << std::endl;
        utils::printHex(buffer);
        ssize_t n = send(sock, buffer.data(), buffer.size(), 0);
        if (n < 0) {
            perror("send");
        }
    });

    client.sendHello();
    std::cout << "HELLO sent" << std::endl;
    std::cout << "something" << std::endl;
    uint8_t recvBuf[4096];
    while (true) {
        ssize_t n = recv(sock, recvBuf, sizeof(recvBuf), 0);
        if (n == 0) {
            std::cout << "Server closed connection\n";
            break;
        } else if (n < 0) {
            perror("recv");
            break;
        }

        // Feed into client buffer
        utils::printHex(std::span(recvBuf, n));
        client.appendRecvBuffer(std::span(recvBuf, n));
        client.processIncoming();

        if (client.getAuthStatus()) {
            std::cout << "Client authenticated!\n";
            break;
            // Example: send a test order once authenticated
            // client.sendTestOrder();
        }
    }

    close(sock);
    return 0;
}