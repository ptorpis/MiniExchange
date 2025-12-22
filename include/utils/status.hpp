#pragma once

#include <cstdint>
namespace status {
enum class HelloAckStatus : std::uint8_t { ACCEPTED = 1, REJECTED = 2 };
enum class LogoutAckStatus : std::uint8_t { ACCEPTED = 1, REJECTED = 2 };
enum class OrderAckStatus : std::uint8_t { ACCEPTED = 1, REJECTED = 2 };
} // namespace status
