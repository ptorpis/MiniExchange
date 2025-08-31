#pragma once

#include <cstdint>

namespace statusCodes {
enum class HelloStatus : uint8_t {
    NULLSTATUS = 0x00,
    ACCEPTED = 0x01,
    INVALID_HMAC = 0x02,
    INVALID_API_KEY = 0x03,
    OUT_OF_ORDER = 0x04,
    ILL_FORMED = 0x05
};

enum class LogoutStatus : uint8_t {
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

} // namespace statusCodes