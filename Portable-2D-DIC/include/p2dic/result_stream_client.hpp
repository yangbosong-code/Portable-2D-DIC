#pragma once

#include "p2dic/dic_engine.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace p2dic {

[[nodiscard]] std::vector<std::uint8_t> receive_latest_packet(
    const std::string& host,
    std::uint16_t port,
    std::uint32_t maximum_packet_size,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(3000));

// Connects to a result stream and receives one complete, verified point field.
// Primarily used by diagnostics and end-to-end tests; Studio uses Qt sockets.
[[nodiscard]] DicResult receive_latest_result(
    const std::string& host,
    std::uint16_t port,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(3000));

}  // namespace p2dic
