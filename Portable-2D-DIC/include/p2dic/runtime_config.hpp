#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace p2dic {

struct RuntimeConfig {
    std::uint32_t width{4504};
    std::uint32_t height{4096};
    double camera_fps{21.4};
    int subset_radius{20};
    int grid_step{32};
    int search_radius{8};
    std::size_t frame_pool_size{8};
    std::size_t capture_queue_size{3};

    void validate() const {
        if (width < 64 || height < 64) {
            throw std::invalid_argument("Image dimensions are too small");
        }
        if (camera_fps <= 0.0) {
            throw std::invalid_argument("Camera FPS must be positive");
        }
        if (subset_radius < 2 || grid_step < 1 || search_radius < 0) {
            throw std::invalid_argument("DIC grid parameters are invalid");
        }
        if (frame_pool_size < capture_queue_size + 3) {
            throw std::invalid_argument("Frame pool must exceed queue capacity by at least three");
        }
    }
};

}  // namespace p2dic
