#pragma once

#ifdef ENABLE_LOGGING
#include "events/eventBus.hpp"
#include "logger/eventLogger.hpp"
#include <filesystem>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#define ADD_LOGGER(EventType, FileName)                                                  \
    loggerFactories[#EventType] = [&]() {                                                \
        return std::make_shared<GenericEventLogger<EventType>>(evBus,                    \
                                                               runDir / FileName);       \
    };

inline std::vector<std::shared_ptr<void>>
createLoggers(std::shared_ptr<EventBus> evBus, const std::filesystem::path& runDir) {
    std::unordered_map<std::string, std::function<std::shared_ptr<void>()>>
        loggerFactories;

    ADD_LOGGER(ReceiveMessageEvent, "recv_messages.csv")
    ADD_LOGGER(AddedToBookEvent, "added_to_book.csv")
    ADD_LOGGER(OrderCancelledEvent, "cancelled_orders.csv")
    ADD_LOGGER(ModifyEvent, "modified_orders.csv")
    ADD_LOGGER(TradeEvent, "trades.csv")
    ADD_LOGGER(RemoveFromBookEvent, "removed_from_book.csv")
    ADD_LOGGER(NewConnectionEvent, "new_connections.csv")
    ADD_LOGGER(DisconnectEvent, "disconnects.csv")
    ADD_LOGGER(SendMessageEvent, "sent_messages.csv")

    std::vector<std::shared_ptr<void>> loggers;
    for (auto& [eventName, factory] : loggerFactories) {
        loggers.push_back(factory());
    }
    return loggers;
}
#endif
