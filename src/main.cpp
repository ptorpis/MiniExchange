#include <algorithm>
#include <arpa/inet.h>
#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>
#include <vector>

#include "api/api.hpp"
#include "auth/sessionManager.hpp"
#include "client/client.hpp"
#include "core/matchingEngine.hpp"
#include "core/order.hpp"
#include "network/networkHandler.hpp"
#include "utils/types.hpp"
#include "utils/utils.hpp"

int main() {
    int listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 2;

    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(12345);

    if (bind(listenFd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    if (listen(listenFd, SOMAXCONN) < 0) {
        perror("listen");
        return 1;
    }

    int epollFD = epoll_create1(0);
    if (epollFD < 0) {
        perror("epoll_create1");
        return 1;
    }

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = listenFd;
    epoll_ctl(epollFD, EPOLL_CTL_ADD, listenFd, &ev);
    MatchingEngine engine;
    SessionManager sessionManager;
    OrderService service;
    MiniExchangeAPI api(engine, sessionManager, service);
    NetworkHandler handler(
        api, sessionManager, [](Session& session, const std::span<const uint8_t> buffer) {
            ssize_t sent = ::send(session.FD, buffer.data(), buffer.size(), 0);
            if (sent < 0) {
                perror("send");
                return;
            } else if (sent < static_cast<ssize_t>(buffer.size())) {
                std::cout << "server sending this" << std::endl;
                utils::printHex(buffer);
                session.sendBuffer.insert(session.sendBuffer.end(), buffer.begin() + sent,
                                          buffer.end());
            }
        });

    std::vector<uint8_t> ioBuffer(4096);

    std::cout << "server running" << std::endl;

    while (true) {
        epoll_event events[64];
        int n = epoll_wait(epollFD, events, 64, -1);
        if (n < 0) {
            perror("epoll_wait");
            break;
        }

        for (int i{}; i < n; ++i) {
            int fd = events[i].data.fd;

            if (fd == listenFd) {
                sockaddr_in clientAddr{};
                socklen_t clientLen = sizeof(clientAddr);
                int clientFD = accept(listenFd, (sockaddr*)&clientAddr, &clientLen);
                if (clientFD >= 0) {
                    std::cout << "New Connection: " << clientFD << std::endl;

                    epoll_event clientEv{};
                    clientEv.events = EPOLLIN | EPOLLOUT | EPOLLET;
                    clientEv.data.fd = clientFD;
                    epoll_ctl(epollFD, EPOLL_CTL_ADD, clientFD, &clientEv);
                    uint8_t tmp[4096];
                    ssize_t count = recv(clientFD, tmp, sizeof(tmp), 0);
                    std::cout << "count " << count << std::endl;

                    Session& sess = sessionManager.createSession(clientFD);

                    std::fill(sess.hmacKey.begin(), sess.hmacKey.end(), 0x11);

                    if (count > 0) {
                        sess.recvBuffer.insert(sess.recvBuffer.end(), tmp, tmp + count);

                        handler.onMessage(clientFD);
                    }
                } else {
                    if (events[i].events & EPOLLIN) {
                        // Incoming data
                        Session* session = sessionManager.getSession(fd);
                        if (!session) continue;

                        ssize_t count = recv(fd, ioBuffer.data(), ioBuffer.size(), 0);
                        if (count <= 0) {
                            std::cout << "Client " << fd << " disconnected\n";
                            handler.onDisconnect(fd);
                            close(fd);
                            sessionManager.removeSession(fd);
                            continue;
                        }

                        // Append into recvBuffer
                        session->recvBuffer.insert(session->recvBuffer.end(),
                                                   ioBuffer.begin(),
                                                   ioBuffer.begin() + count);

                        handler.onMessage(fd);
                    }
                    if (events[i].events & EPOLLOUT) {
                        // Flush outgoing
                        Session* session = sessionManager.getSession(fd);
                        if (!session) continue;

                        if (!session->sendBuffer.empty()) {
                            ssize_t sent = send(fd, session->sendBuffer.data(),
                                                session->sendBuffer.size(), 0);
                            if (sent > 0) {
                                session->sendBuffer.erase(session->sendBuffer.begin(),
                                                          session->sendBuffer.begin() +
                                                              sent);
                            }
                        }
                    }

                    if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                        std::cout << "Client " << fd << " error/hangup\n";
                        handler.onDisconnect(fd);
                        close(fd);
                        sessionManager.removeSession(fd);
                    }
                }
            }
        }
    }

    close(listenFd);
    close(epollFD);
    return 0;
}