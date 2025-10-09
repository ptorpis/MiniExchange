#pragma once

#include <cstdint>

namespace statusCodes {
enum class HelloAckStatus : uint8_t {
    NULLSTATUS = 0x00,
    ACCEPTED = 0x01,
    INVALID_HMAC = 0x02,
    INVALID_API_KEY = 0x03,
    OUT_OF_ORDER = 0x04,
    ILL_FORMED = 0x05
};

enum class LogoutAckStatus : uint8_t {
    NULLSTATUS = 0x00,
    ACCEPTED = 0x01,
    INVALID_HMAC = 0x02,
    OUT_OF_ORDER = 0x04
};

enum class OrderAckStatus : uint8_t {
    NULLSTATUS = 0x00,
    ACCEPTED = 0x01,
    INVALID = 0x02,
    OUT_OF_ORDER = 0x03,
    NOT_AUTHENTICATED = 0x04
};

enum class CancelAckStatus : uint8_t {
    NULLSTATUS = 0x00,
    ACCEPTED = 0x01,
    INVALID = 0x02,
    NOT_FOUND = 0x03,
    NOT_AUTHENTICATED = 0x04,
    OUT_OF_ORDER = 0x05
};

enum class ModifyAckStatus : uint8_t {
    NULLSTATUS = 0x00,
    ACCEPTED = 0x01,
    INVALID = 0x02,
    NOT_FOUND = 0x03,
    NOT_AUTHENTICATED = 0x04,
    OUT_OF_ORDER = 0x05
};

enum class OrderStatus : uint8_t {
    NULLSTATUS = 0x00,
    NEW = 0x01,
    REJECTED = 0x02,
    PARTIALLY_FILLED = 0x03,
    FILLED = 0x04,
    CANCELLED = 0x05,
    MODIFIED = 0x06
};

constexpr const char* toStr(OrderStatus s) {
    switch (s) {
    case OrderStatus::NEW:
        return "NEW";
    case OrderStatus::REJECTED:
        return "REJECTED";
    case OrderStatus::PARTIALLY_FILLED:
        return "PARTIALLY_FILLED";
    case OrderStatus::FILLED:
        return "FILLED";
    case OrderStatus::CANCELLED:
        return "CANCELLED";
    case OrderStatus::MODIFIED:
        return "MODIFIED";
    default:
        return "UNKNOWN_ORDER_STATUS";
    }
}

constexpr const char* toStr(HelloAckStatus s) {
    switch (s) {
    case HelloAckStatus::NULLSTATUS:
        return "NULLSTATUS";
    case HelloAckStatus::ACCEPTED:
        return "ACCEPTED";
    case HelloAckStatus::INVALID_HMAC:
        return "INVALID_HMAC";
    case HelloAckStatus::INVALID_API_KEY:
        return "INVALID_API_KEY";
    case HelloAckStatus::OUT_OF_ORDER:
        return "OUT_OF_ORDER";
    case HelloAckStatus::ILL_FORMED:
        return "ILL_FORMED";
    default:
        return "UNKNOWN_HELLO_STATUS";
    }
}

constexpr const char* toStr(LogoutAckStatus s) {
    switch (s) {
    case LogoutAckStatus::NULLSTATUS:
        return "NULLSTATUS";
    case LogoutAckStatus::ACCEPTED:
        return "ACCEPTED";
    case LogoutAckStatus::INVALID_HMAC:
        return "INVALID_HMAC";
    case LogoutAckStatus::OUT_OF_ORDER:
        return "OUT_OF_ORDER";
    default:
        return "UNKNOWN_LOGOUT_STATUS";
    }
}

constexpr const char* toStr(OrderAckStatus s) {
    switch (s) {
    case OrderAckStatus::NULLSTATUS:
        return "NULLSTATUS";
    case OrderAckStatus::ACCEPTED:
        return "ACCEPTED";
    case OrderAckStatus::INVALID:
        return "INVALID";
    case OrderAckStatus::OUT_OF_ORDER:
        return "OUT_OF_ORDER";
    case OrderAckStatus::NOT_AUTHENTICATED:
        return "NOT_AUTHENTICATED";
    default:
        return "UNKNOWN_ORDER_ACK_STATUS";
    }
}

constexpr const char* toStr(CancelAckStatus s) {
    switch (s) {
    case CancelAckStatus::NULLSTATUS:
        return "NULLSTATUS";
    case CancelAckStatus::ACCEPTED:
        return "ACCEPTED";
    case CancelAckStatus::INVALID:
        return "INVALID";
    case CancelAckStatus::NOT_FOUND:
        return "NOT_FOUND";
    case CancelAckStatus::NOT_AUTHENTICATED:
        return "NOT_AUTHENTICATED";
    case CancelAckStatus::OUT_OF_ORDER:
        return "OUT_OF_ORDER";
    default:
        return "UNKNOWN_CANCEL_ACK_STATUS";
    }
}

constexpr const char* toStr(ModifyAckStatus s) {
    switch (s) {
    case ModifyAckStatus::NULLSTATUS:
        return "NULLSTATUS";
    case ModifyAckStatus::ACCEPTED:
        return "ACCEPTED";
    case ModifyAckStatus::INVALID:
        return "INVALID";
    case ModifyAckStatus::NOT_FOUND:
        return "NOT_FOUND";
    case ModifyAckStatus::NOT_AUTHENTICATED:
        return "NOT_AUTHENTICATED";
    case ModifyAckStatus::OUT_OF_ORDER:
        return "OUT_OF_ORDER";
    default:
        return "UNKNOWN_MODIFY_STATUS";
    }
}

} // namespace statusCodes
