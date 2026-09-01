#include "p2dic/synthetic_camera.hpp"
#include "p2dic/zncc.hpp"

#include <chrono>
#include <iostream>

int main() {
    using namespace std::chrono_literals;

    p2dic::SyntheticCameraConfig config;
    config.frames_per_second = 1000.0;
    config.displacement_x_per_frame = 4;
    config.displacement_y_per_frame = -3;

    p2dic::SyntheticCamera camera(config);
    p2dic::Frame first;
    p2dic::Frame second;
    camera.open();
    camera.start();

    if (!camera.grab(first, 100ms) || !camera.grab(second, 100ms)) {
        std::cerr << "Synthetic acquisition timed out\n";
        return 1;
    }

    const auto result = p2dic::find_integer_translation_zncc(
        first, second, 256, 256, 18, 7);

    if (result.dx != 4 || result.dy != -3 || result.score < 0.999) {
        std::cerr << "Expected (4, -3, score >= 0.999), got ("
                  << result.dx << ", " << result.dy << ", " << result.score << ")\n";
        return 2;
    }
    return 0;
}
