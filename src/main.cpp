#include "client/client.hpp"
#include "events/eventBus.hpp"
#include "protocol/client/clientMessageFactory.hpp"
#include "protocol/messages.hpp"
#include "protocol/protocolHandler.hpp"
#include "protocol/traits.hpp"
#include "server/server.hpp"

#include <iostream>
#include <memory>

int main() {
    auto evBus = std::make_shared<EventBus>();
    evBus->start();
    evBus->subscribe<AddedToBookEvent>([](const ServerEvent<AddedToBookEvent>& ev) {
        const auto& e = ev.event;
        std::cout << "[EVENT][ADDED] ts=" << ev.tsNs << " order=" << e.orderID
                  << " client=" << e.clientID << " side=" << static_cast<int>(e.side)
                  << " qty=" << e.qty << " price=" << e.price << std::endl;
    });

    evBus->subscribe<OrderCancelledEvent>([](const ServerEvent<OrderCancelledEvent>& ev) {
        std::cout << "[EVENT][CANCELLED] ts=" << ev.tsNs << " order=" << ev.event.orderID
                  << std::endl;
    });

    evBus->subscribe<ModifyEvent>([](const ServerEvent<ModifyEvent>& ev) {
        const auto& e = ev.event;
        std::cout << "[EVENT][MODIFIED] ts=" << ev.tsNs << " oldOrder=" << e.oldOrderID
                  << " newOrder=" << e.newOrderID << " qty=" << e.newQty
                  << " price=" << e.newPrice << std::endl;
    });

    evBus->subscribe<TradeEvent>([](const ServerEvent<TradeEvent>& ev) {
        const auto& e = ev.event;
        std::cout << "[EVENT][TRADE] ts=" << ev.tsNs << " tradeID=" << e.tradeID
                  << " buyerOrder=" << e.buyerOrderID
                  << " sellerOrder=" << e.sellerOrderID << " buyer=" << e.buyerID
                  << " seller=" << e.sellerID << " qty=" << e.qty << " price=" << e.price
                  << " instrument=" << e.instrumentID << std::endl;
    });

    evBus->subscribe<RemoveFromBookEvent>([](const ServerEvent<RemoveFromBookEvent>& ev) {
        const auto& e = ev.event;
        std::cout << "[EVENT][REMOVE FROM BOOK] ts=" << ev.tsNs
                  << " orderID=" << e.orderID << std::endl;
    });

    evBus->subscribe<ReceiveMessageEvent>([](const ServerEvent<ReceiveMessageEvent>& ev) {
        std::cout << "[MSG] ts=" << ev.tsNs << " fd=" << ev.event.fd
                  << " clientID=" << ev.event.clientID
                  << " type=" << static_cast<int>(ev.event.type) << std::endl;
    });

    SessionManager sessionManager;
    ProtocolHandler handler(sessionManager, evBus);
    uint16_t port = 12345;

    Server server(port, sessionManager, handler, evBus);
    if (!server.start(port)) {
        return 1;
    }
    server.run();

    evBus->stop();
    return 0;
}