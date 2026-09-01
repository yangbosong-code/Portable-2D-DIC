#pragma once

#include "p2dic/frame_buffer.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace p2dic {

struct Frame {
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t stride{};
    std::uint64_t sequence{};
    std::uint64_t timestamp_ns{};
    FrameBuffer pixels;

    Frame() = default;

    Frame(std::uint32_t image_width, std::uint32_t image_height)
        : width(image_width),
          height(image_height),
          stride(image_width),
          pixels(static_cast<std::size_t>(image_width) * image_height) {}

    [[nodiscard]] std::uint8_t at(std::uint32_t x, std::uint32_t y) const {
        if (x >= width || y >= height) {
            throw std::out_of_range("Frame pixel coordinate is out of range");
        }
        return pixels[static_cast<std::size_t>(y) * stride + x];
    }
};

}  // namespace p2dic
