#pragma once

#include "p2dic/camera.hpp"

#include <cstdint>
#include <random>

namespace p2dic {

enum class SyntheticMotionMode { linear, sinusoidal };

struct SyntheticCameraConfig {
    std::uint32_t width{512};
    std::uint32_t height{512};
    double frames_per_second{20.0};
    double displacement_x_per_frame{1.0};
    double displacement_y_per_frame{0.0};
    SyntheticMotionMode motion_mode{SyntheticMotionMode::linear};
    double displacement_x_amplitude{2.0};
    double displacement_y_amplitude{1.0};
    double motion_frequency_hz{0.1};
    std::uint32_t seed{20260808};
};

class SyntheticCamera final : public ICamera {
public:
    explicit SyntheticCamera(SyntheticCameraConfig config = {});

    void open() override;
    void start() override;
    bool grab(Frame& destination, std::chrono::milliseconds timeout) override;
    void stop() override;
    void close() override;
    [[nodiscard]] std::string_view name() const noexcept override;

private:
    void generate_reference();
    void apply_optical_blur();
    void render_shifted(Frame& destination, double dx, double dy) const;

    SyntheticCameraConfig config_;
    Frame reference_;
    std::uint64_t sequence_{0};
    bool opened_{false};
    bool streaming_{false};
    std::mt19937 random_;
};

}  // namespace p2dic
