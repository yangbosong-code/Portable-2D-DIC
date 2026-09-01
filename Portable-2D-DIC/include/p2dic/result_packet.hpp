#pragma once

#include "p2dic/dic_engine.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace p2dic {

inline constexpr std::uint16_t result_packet_version = 1;
inline constexpr std::uint16_t result_packet_header_size = 48;
inline constexpr std::uint32_t result_packet_point_size = 36;

// Portable little-endian wire representation used between DIC Edge and Studio.
// Throws std::invalid_argument/std::runtime_error when input is invalid.
[[nodiscard]] std::vector<std::uint8_t> encode_result_packet(const DicResult& result);
[[nodiscard]] DicResult decode_result_packet(std::span<const std::uint8_t> packet);

}  // namespace p2dic
