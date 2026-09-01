#pragma once

#include "p2dic/frame.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace p2dic {

struct PreviewFrame {
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint64_t sequence{};
    std::uint64_t timestamp_ns{};
    std::vector<std::uint8_t> pixels;
};

inline constexpr std::uint16_t preview_packet_version = 1;
inline constexpr std::uint16_t preview_packet_header_size = 40;

[[nodiscard]] PreviewFrame make_preview_frame(
    const Frame& source, std::uint32_t maximum_width, std::uint32_t maximum_height);
[[nodiscard]] std::vector<std::uint8_t> encode_preview_packet(const PreviewFrame& preview);
[[nodiscard]] PreviewFrame decode_preview_packet(std::span<const std::uint8_t> packet);

}  // namespace p2dic
