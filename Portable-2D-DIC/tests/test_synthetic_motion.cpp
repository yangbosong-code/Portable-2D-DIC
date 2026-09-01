#include "p2dic/synthetic_camera.hpp"
#include "p2dic/hybrid_grid_engine.hpp"
#include "p2dic/zncc.hpp"

#include <chrono>
#include <cmath>
#include <iostream>

int main() {
    using namespace std::chrono_literals;

    p2dic::SyntheticCameraConfig config;
    config.width = 256;
    config.height = 256;
    config.frames_per_second = 500.0;
    config.motion_mode = p2dic::SyntheticMotionMode::sinusoidal;
    config.displacement_x_amplitude = 2.0;
    config.displacement_y_amplitude = 1.0;
    config.motion_frequency_hz = 10.0;

    p2dic::SyntheticCamera camera(config);
    p2dic::Frame reference;
    p2dic::Frame frame;
    camera.open();
    camera.start();
    if (!camera.grab(reference, 50ms)) {
        std::cerr << "Unable to acquire synthetic reference\n";
        return 1;
    }

    p2dic::HybridGridEngine engine(p2dic::HybridGridConfig{18, 64, 6, 25, 1.0e-2, 0.80});
    double minimum_score = 1.0;
    double minimum_valid_ratio = 1.0;
    for (int index = 1; index <= 100; ++index) {
        if (!camera.grab(frame, 50ms)) {
            std::cerr << "Synthetic acquisition timed out at frame " << index << '\n';
            return 2;
        }
        const auto match = p2dic::find_integer_translation_zncc(
            reference, frame, 128, 128, 18, 6);
        minimum_score = std::min(minimum_score, match.score);
        if (std::abs(match.dx) > 3 || std::abs(match.dy) > 2 || match.score < 0.85) {
            std::cerr << "Bounded motion lost at frame " << index << ": ("
                      << match.dx << ", " << match.dy << ", " << match.score << ")\n";
            return 3;
        }
        const auto grid = engine.process(reference, frame);
        std::size_t valid = 0;
        for (const auto& point : grid.points) valid += point.valid ? 1U : 0U;
        const double valid_ratio = grid.points.empty()
                                       ? 0.0
                                       : static_cast<double>(valid) / grid.points.size();
        minimum_valid_ratio = std::min(minimum_valid_ratio, valid_ratio);
    }
    camera.stop();
    camera.close();
    std::cout << "minimum_zncc=" << minimum_score
              << " minimum_valid_ratio=" << minimum_valid_ratio << '\n';
    return minimum_valid_ratio >= 0.95 ? 0 : 4;
}
