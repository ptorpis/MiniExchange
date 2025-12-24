#include "protocol/clientMessages.hpp"
#include "protocol/serverMessages.hpp"
#include <gtest/gtest.h>
#include <string>

using namespace client;
using namespace server;

template <typename T, typename Field> std::size_t byteOffset(T& obj, Field& field) {
    auto* base = reinterpret_cast<std::byte*>(&obj);
    auto* addr = reinterpret_cast<std::byte*>(&field);

    std::ptrdiff_t diff = addr - base;
    EXPECT_GE(diff, 0) << "Negative field offset detected";

    return static_cast<std::size_t>(diff);
}

template <typename Payload> void expectStandardPackedLayout(Payload& payload) {
    std::size_t expectedOffset = 0;

    payload.iterateElementsWithNames([&](const char* name, auto& field) {
        using FieldT = std::remove_reference_t<decltype(field)>;

        std::size_t actualOffset = byteOffset(payload, field);

        EXPECT_EQ(actualOffset, expectedOffset)
            << "Field '" << name << "' is out of order";

        expectedOffset += sizeof(FieldT);
    });

    EXPECT_EQ(expectedOffset, sizeof(Payload))
        << "Total field size does not match sizeof(Payload)";
}

TEST(ClientPayloads, NewOrderPayload_IteratesWithValues) {
    NewOrderPayload payload{};
    payload.serverClientID = 1;
    payload.instrumentID = 2;
    payload.orderSide = +(OrderSide::BUY);
    payload.orderType = +(OrderType::LIMIT);
    payload.timeInForce = 5;
    payload.qty = 100;
    payload.price = 200;
    payload.goodTillDate = 999;

    payload.iterateElementsWithNames([&](const char* name, auto& field) {
        std::string fieldName(name);
        if (fieldName == "padding") {
            return;
        }
        if (fieldName == "serverClientID") {
            EXPECT_EQ(field, 1);
        } else if (fieldName == "instrumentID") {
            EXPECT_EQ(field, 2);
        } else if (fieldName == "orderSide") {
            EXPECT_EQ(field, +OrderSide::BUY);
        } else if (fieldName == "qty") {
            EXPECT_EQ(field, 100);
        } else if (fieldName == "price") {
            EXPECT_EQ(field, 200);
        } else if (fieldName == "goodTillDate") {
            EXPECT_EQ(field, 999);
        }
    });
}

TEST(ServerPayloads, HelloAckPayload_IteratesWithValues) {
    HelloAckPayload payload{};
    payload.serverClientID = 123;
    payload.status = 7;

    payload.iterateElementsWithNames([&](const char* name, auto& field) {
        using FieldT = std::remove_reference_t<decltype(field)>;

        if constexpr (std::is_array_v<FieldT>) {
            // padding
            return;
        } else {
            std::string n{name};
            if (n == "serverClientID")
                EXPECT_EQ(field, 123);
            else {
                EXPECT_EQ(field, 7);
            }
        }
    });
}

TEST(ServerPayloads, OrderAckPayload_IteratesWithValues) {
    OrderAckPayload payload{};
    payload.serverClientID = 1;
    payload.serverOrderID = 2;
    payload.acceptedPrice = 100;
    payload.remainingQty = 50;
    payload.serverTime = 999;
    payload.instrumentID = 42;
    payload.status = 3;

    payload.iterateElementsWithNames([&](const char* name, auto& field) {
        using FieldT = std::remove_reference_t<decltype(field)>;

        if constexpr (std::is_array_v<FieldT>) {
            return;
        } else {
            std::string n{name};

            if (n == "serverClientID") {
                EXPECT_EQ(field, 1);
            } else if (n == "serverOrderID") {
                EXPECT_EQ(field, 2);
            } else if (n == "acceptedPrice") {
                EXPECT_EQ(field, 100);
            } else if (n == "remainingQty") {
                EXPECT_EQ(field, 50);
            } else if (n == "serverTime") {
                EXPECT_EQ(field, 999);
            } else if (n == "instrumentID") {
                EXPECT_EQ(field, 42);
            } else if (n == "status") {
                EXPECT_EQ(field, 3);
            } else {
                FAIL() << "Unexpected field: " << n;
            }
        }
    });
}

TEST(ClientPayloadLayout, HelloPayload_Layout) {
    client::HelloPayload payload{};
    expectStandardPackedLayout(payload);
}

TEST(ClientPayloadLayout, LogoutPayload_Layout) {
    client::LogoutPayload payload{};
    expectStandardPackedLayout(payload);
}

TEST(ClientPayloadLayout, NewOrderPayload_Layout) {
    client::NewOrderPayload payload{};
    expectStandardPackedLayout(payload);
}

TEST(ClientPayloadLayout, CancelOrderPayload_Layout) {
    client::CancelOrderPayload payload{};
    expectStandardPackedLayout(payload);
}

TEST(ClientPayloadLayout, ModifyOrderPayload_Layout) {
    client::ModifyOrderPayload payload{};
    expectStandardPackedLayout(payload);
}

TEST(ServerPayloadLayout, HelloAckPayload_Layout) {
    server::HelloAckPayload payload{};
    expectStandardPackedLayout(payload);
}

TEST(ServerPayloadLayout, LogoutAckPayload_Layout) {
    server::LogoutAckPayload payload{};
    expectStandardPackedLayout(payload);
}

TEST(ServerPayloadLayout, OrderAckPayload_Layout) {
    server::OrderAckPayload payload{};
    expectStandardPackedLayout(payload);
}

TEST(ServerPayloadLayout, CancelAckPayload_Layout) {
    server::CancelAckPayload payload{};
    expectStandardPackedLayout(payload);
}

TEST(ServerPayloadLayout, ModifyAckPayload_Layout) {
    server::ModifyAckPayload payload{};
    expectStandardPackedLayout(payload);
}

TEST(ServerPayloadLayout, TradePayload_Layout) {
    server::TradePayload payload{};
    expectStandardPackedLayout(payload);
}
