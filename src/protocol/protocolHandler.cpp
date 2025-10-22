#include "protocol/client/clientMessages.hpp"
#include "protocol/messages.hpp"
#include "protocol/protocolHandler.hpp"
#include "protocol/serialize.hpp"
#include "protocol/statusCodes.hpp"
#include "protocol/traits.hpp"
#include "utils/orderBookRenderer.hpp"
#include "utils/timing.hpp"
#include "utils/utils.hpp"

#include <arpa/inet.h>
#include <chrono>
#include <iostream>
#include <stdexcept>

using MsgEvent = ServerEvent<ReceiveMessageEvent>;

void ProtocolHandler::onMessage(int fd) {
    outBoundFDs_.clear();
    Session* session = api_.getSession(fd);
    if (!session) {
        return;
    }
    while (true) {
        std::optional<MessageHeader> headerOpt = peekHeader_(*session);
        if (!headerOpt) {
            return;
        }
        MessageHeader header = headerOpt.value();
        MessageType type = static_cast<MessageType>(header.messageType);
        uint8_t versionFlag = header.protocolVersionFlag;
        if (versionFlag != (+HeaderFlags::PROTOCOL_VERSION)) {
            api_.disconnectClient(fd);
            return;
        }

        size_t totalSize = 0;

#ifdef ENABLE_TIMING
        // Sampling logic - only record every Nth message
        bool shouldSample = false;
        if (--timingSampleCounter_ <= 0) {
            shouldSample = true;
            timingSampleCounter_ = TIMING_SAMPLE_INTERVAL; // Reset counter
        }

        TimingRecord timingRec;
        if (shouldSample) {
            timingRec.tReceived = TSCClock::now();
            timingRec.messageType = +type;
        }
#endif

        switch (type) {
        case MessageType::HELLO: {
            totalSize = constants::HEADER_SIZE +
                        client::PayloadTraits<client::HelloPayload>::size;
            if (session->recvBuffer.size() < totalSize) {
                return; // wait for more data
            }

            evBus_->publish<ReceiveMessageEvent>(
                MsgEvent{TSCClock::now(),
                         {fd, session->serverClientID, +type, header.clientMsgSqn}});

            if (session->authenticated) {
                break;
            }

            auto msgOpt = deserializeMessage<client::HelloPayload>(session->recvBuffer);
            if (!msgOpt.has_value()) {
                return;
            }

#ifdef ENABLE_TIMING
            if (shouldSample) {
                timingRec.tDeserialized = TSCClock::now();
            }
#endif

            if (!session) {
                return;
            }
            sendFn_(*session, api_.handleHello(*session, msgOpt.value()));
            outBoundFDs_.push_back(session->FD);

#ifdef ENABLE_TIMING
            if (shouldSample) {
                timingRec.tBuffered = TSCClock::now();
                utils::recordTiming(timingRec);
            }
#endif

            break;
        }
        case MessageType::LOGOUT: {
            totalSize = constants::HEADER_SIZE +
                        client::PayloadTraits<client::LogoutPayload>::size;
            if (session->recvBuffer.size() < totalSize) {
                return;
            }

            evBus_->publish<ReceiveMessageEvent>(
                MsgEvent{TSCClock::now(),
                         {fd, session->serverClientID, +type, header.clientMsgSqn}});

            if (!session->authenticated) {
                break;
            }

            if (auto msgOpt =
                    deserializeMessage<client::LogoutPayload>(session->recvBuffer)) {
#ifdef ENABLE_TIMING
                if (shouldSample) {
                    timingRec.tDeserialized = TSCClock::now();
                }
#endif
                sendFn_(*session, api_.handleLogout(*session, msgOpt.value()));
                outBoundFDs_.push_back(session->FD);
            }

#ifdef ENABLE_TIMING
            if (shouldSample) {
                timingRec.tBuffered = TSCClock::now();
                utils::recordTiming(timingRec);
            }
#endif

            break;
        }

        case MessageType::NEW_ORDER: {
            totalSize = constants::HEADER_SIZE +
                        client::PayloadTraits<client::NewOrderPayload>::size;
            if (session->recvBuffer.size() < totalSize) {
                return;
            }

            evBus_->publish<ReceiveMessageEvent>(
                MsgEvent{TSCClock::now(),
                         {fd, session->serverClientID, +type, header.clientMsgSqn}});

            if (!session->authenticated) {
                break;
            }

            if (auto msgOpt =
                    deserializeMessage<client::NewOrderPayload>(session->recvBuffer)) {
#ifdef ENABLE_TIMING
                if (shouldSample) {
                    timingRec.tDeserialized = TSCClock::now();
                }
#endif
                if (!session) {
                    return;
                }
                auto responses = api_.handleNewOrder(*session, msgOpt.value());
                for (auto& response : responses) {
                    Session* sess = api_.getSession(response.fd);
                    sendFn_(*sess, response.data);
                    outBoundFDs_.push_back(response.fd);
                }
            }

#ifdef ENABLE_TIMING
            if (shouldSample) {
                timingRec.tBuffered = TSCClock::now();
                utils::recordTiming(timingRec);
            }
#endif
            break;
        }

        case MessageType::CANCEL_ORDER: {
            totalSize = constants::HEADER_SIZE +
                        client::PayloadTraits<client::CancelOrderPayload>::size;
            if (session->recvBuffer.size() < totalSize) {
                return;
            }

            evBus_->publish<ReceiveMessageEvent>(
                MsgEvent{TSCClock::now(),
                         {fd, session->serverClientID, +type, header.clientMsgSqn}});

            if (!session->authenticated) {
                break;
            }

            if (auto msgOpt =
                    deserializeMessage<client::CancelOrderPayload>(session->recvBuffer)) {
#ifdef ENABLE_TIMING
                if (shouldSample) {
                    timingRec.tDeserialized = TSCClock::now();
                }
#endif

                if (!session) {
                    return;
                }
                sendFn_(*session, api_.handleCancel(*session, msgOpt.value()));
                outBoundFDs_.push_back(session->FD);
            }

#ifdef ENABLE_TIMING
            if (shouldSample) {
                timingRec.tBuffered = TSCClock::now();
                utils::recordTiming(timingRec);
            }
#endif

            break;
        }

        case MessageType::MODIFY_ORDER: {
            totalSize = constants::HEADER_SIZE +
                        client::PayloadTraits<client::ModifyOrderPayload>::size;

            if (session->recvBuffer.size() < totalSize) {
                return;
            }

            evBus_->publish<ReceiveMessageEvent>(
                MsgEvent{TSCClock::now(),
                         {fd, session->serverClientID, +type, header.clientMsgSqn}});

            if (!session->authenticated) {
                break;
            }

            if (auto msgOpt =
                    deserializeMessage<client::ModifyOrderPayload>(session->recvBuffer)) {
#ifdef ENABLE_TIMING
                if (shouldSample) {
                    timingRec.tDeserialized = TSCClock::now();
                }
#endif
                if (!session) {
                    return;
                }

                auto responses = api_.handleModify(*session, msgOpt.value());
                for (auto& response : responses) {
                    sendFn_(*(api_.getSession(response.fd)), response.data);
                    outBoundFDs_.push_back(response.fd);
                }
            }

#ifdef ENABLE_TIMING
            if (shouldSample) {
                timingRec.tBuffered = TSCClock::now();
                utils::recordTiming(timingRec);
            }
#endif
            break;
        }

        case MessageType::HEARTBEAT: {
            totalSize = client::PayloadTraits<client::HeartBeatPayload>::msgSize;
            if (session->recvBuffer.size() < totalSize) {
                return;
            }

            if (!session) return;
            if (session->recvBuffer.size() < totalSize) return;

            evBus_->publish<ReceiveMessageEvent>(
                MsgEvent{TSCClock::now(),
                         {fd, session->serverClientID, +type, header.clientMsgSqn}});

            api_.updateHb(session->FD);

#ifdef ENABLE_TIMING
            if (shouldSample) {
                timingRec.tDeserialized = TSCClock::now();
                timingRec.tBuffered = TSCClock::now();
                utils::recordTiming(timingRec);
            }
#endif
            break;
        }

        default: {
            session->recvBuffer.clear();
            evBus_->publish<ReceiveMessageEvent>(
                MsgEvent{TSCClock::now(),
                         {fd, session->serverClientID, +type, header.clientMsgSqn}});
            return;
        }
        }

        session->recvBuffer.erase(session->recvBuffer.begin(),
                                  session->recvBuffer.begin() + totalSize);
    }
}

std::optional<MessageHeader> ProtocolHandler::peekHeader_(Session& session) const {
    if (session.recvBuffer.size() < sizeof(MessageHeader)) {
        return std::nullopt;
    }

    MessageHeader header{};
    std::memcpy(&header, session.recvBuffer.data(), sizeof(MessageHeader));

    header.payLoadLength = ntohs(header.payLoadLength);
    header.clientMsgSqn = ntohl(header.clientMsgSqn);
    header.serverMsgSqn = ntohl(header.serverMsgSqn);

    return header;
}
