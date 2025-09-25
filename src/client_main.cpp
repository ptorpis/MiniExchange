#include "client/client.hpp"
#include "protocol/messages.hpp"

#include <arpa/inet.h>
#include <array>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

int makeNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    const char* serverIP = "127.0.0.1";
    int serverPort = 12345;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket failed");
        return 1;
    }

    if (makeNonBlocking(sock) < 0) {
        perror("makeNonBlocking");
        close(sock);
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
        if (errno != EINPROGRESS) {
            perror("connect failed");
            close(sock);
            return 1;
        }
        // EINPROGRESS is fine for non-blocking
    }

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        close(sock);
        return 1;
    }

    epoll_event event{};
    event.events = EPOLLIN | EPOLLOUT | EPOLLET;
    event.data.fd = sock;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &event) == -1) {
        perror("epoll_ctl");
        close(sock);
        close(epoll_fd);
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

    const int MAX_EVENTS = 64;
    std::vector<epoll_event> events(MAX_EVENTS);

    bool running = true;
    bool helloSent = false;
    bool helloAcked = false;

    while (running && !helloAcked) {
        int n = epoll_wait(epoll_fd, events.data(), MAX_EVENTS, -1);
        if (n == -1) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd == sock) {
                if (events[i].events & EPOLLIN) {
                    uint8_t buffer[4096];
                    ssize_t count = recv(sock, buffer, sizeof(buffer), 0);
                    if (count == -1) {
                        if (errno != EAGAIN) {
                            perror("recv");
                            running = false;
                        }
                    } else if (count == 0) {
                        std::cout << "Server closed connection" << std::endl;
                        running = false;
                    } else {
                        std::cout << "Client received data:" << std::endl;
                        utils::printHex(std::span(buffer, count));
                        client.appendRecvBuffer(std::span(buffer, count));
                        client.processIncoming();

                        if (client.getAuthStatus()) {
                            std::cout << "HELLO acknowledged by the server" << std::endl;
                            helloAcked = true;
                        }
                    }
                }
                if (events[i].events & EPOLLOUT && !helloSent) {
                    client.sendHello();
                    helloSent = true;
                    epoll_event ev{};
                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.fd = sock;
                    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sock, &ev);
                }
            }
        }
    }
    close(epoll_fd);

    close(sock);
    return 0;
}