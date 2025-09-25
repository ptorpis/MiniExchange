#pragma once
#include "protocol/protocolHandler.hpp"
#include <sys/socket.h>

namespace SendFunctions {

inline ProtocolHandler::SendFn bufferOnly() {
    return [](Session& session, const std::span<const uint8_t> buffer) {
        session.sendBuffer.insert(session.sendBuffer.end(), buffer.begin(), buffer.end());
    };
}

inline ProtocolHandler::SendFn socketSend() {
    return [](Session& session, const std::span<const uint8_t> buffer) {
        ssize_t n = ::send(session.FD, buffer.data(), buffer.size(), 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                session.sendBuffer.insert(session.sendBuffer.end(), buffer.begin(),
                                          buffer.end());
            } else {
                perror("send");
            }
        } else if (n < static_cast<ssize_t>(buffer.size())) {
            session.sendBuffer.insert(session.sendBuffer.end(), buffer.begin(),
                                      buffer.end());
        }
    };
}

} // namespace SendFunctions