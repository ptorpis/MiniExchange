#pragma once

#include <cstddef>
#include <span>
#include <vector>

using MessageBuffer = std::vector<std::byte>;
using MessageView = std::span<const std::byte>;

struct SerializedMessage {
    MessageBuffer buffer;

    MessageView span() const { return buffer; }
    std::size_t size() const { return buffer.size(); }
};

struct OutBoundMessage {
    int targetFD;
    SerializedMessage message;
};
