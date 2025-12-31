#pragma once
#include "core/order.hpp"
#include "utils/types.hpp"
#include <memory>

struct OrderBuilder {

    struct Defaults {
        static constexpr OrderID orderID{1};
        static constexpr ClientID clientID{1};
        static constexpr ClientOrderID clientOrderID{3};
        static constexpr Qty qty = Qty{100};
        static constexpr Price price{2000};
        static constexpr Timestamp goodTill{0};
        static constexpr Timestamp timestamp{0};
        static constexpr InstrumentID instrumentID{1};
        static constexpr TimeInForce tif{TimeInForce::GOOD_TILL_CANCELLED};
        static constexpr OrderSide side{OrderSide::BUY};
        static constexpr OrderType type{OrderType::LIMIT};
        static constexpr OrderStatus status{OrderStatus::NEW};
    };

    OrderID orderID{Defaults::orderID};
    ClientID clientID{Defaults::clientID};
    ClientOrderID clientOrderID{Defaults::clientOrderID};
    Qty qty{Defaults::qty};
    Price price{Defaults::price};
    Timestamp goodTill{Defaults::goodTill};
    Timestamp timestamp{Defaults::timestamp};
    InstrumentID instrumentID{Defaults::instrumentID};
    TimeInForce tif{Defaults::tif};
    OrderSide side{Defaults::side};
    OrderType type{Defaults::type};
    OrderStatus status{Defaults::status};

    OrderBuilder& withOrderID(OrderID id) {
        orderID = id;
        return *this;
    }
    OrderBuilder& withClientID(ClientID id) {
        clientID = id;
        return *this;
    }
    OrderBuilder& withClientOrderID(ClientOrderID id) {
        clientOrderID = id;
        return *this;
    }
    OrderBuilder& withQty(Qty q) {
        qty = q;
        return *this;
    }
    OrderBuilder& withPrice(Price p) {
        price = p;
        return *this;
    }
    OrderBuilder& withGoodTill(Timestamp t) {
        goodTill = t;
        return *this;
    }
    OrderBuilder& withTimestamp(Timestamp t) {
        timestamp = t;
        return *this;
    }
    OrderBuilder& withInstrumentID(InstrumentID id) {
        instrumentID = id;
        return *this;
    }
    OrderBuilder& withTIF(TimeInForce t) {
        tif = t;
        return *this;
    }
    OrderBuilder& withSide(OrderSide s) {
        side = s;
        return *this;
    }
    OrderBuilder& withType(OrderType t) {
        if (t == OrderType::MARKET) {
            price = Price{0};
        }
        type = t;
        return *this;
    }
    OrderBuilder& withStatus(OrderStatus s) {
        status = s;
        return *this;
    }

    std::unique_ptr<Order> build() {
        return std::make_unique<Order>(orderID, clientID, clientOrderID, qty, price,
                                       goodTill, timestamp, instrumentID, tif, side, type,
                                       status);
    }
};
