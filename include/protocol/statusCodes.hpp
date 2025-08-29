#pragma once

#include <cstdint>

namespace status {
enum class HelloStatus : uint8_t {
    ACCEPTED = 0x01,
    INVALID_HMAC = 0x01,
    INVALID_API_KEY = 0x02,
};

enum class LogoutStatus : uint8_t {
    ACCEPTED = 0x01,
    INVALID_HMAC = 0x02,
};

} // namespace status