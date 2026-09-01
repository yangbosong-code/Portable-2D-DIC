#include "p2dic/synthetic_camera.hpp"
#include "p2dic/zncc.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>

int main() {
    using namespace std::chrono_literals;

    p2dic::SyntheticCameraConfig config;
    config.width = 512;
    config.height = 512;
    config.frames_per_second = 1000.0;  // Minimize simulated waiting in this benchmark.
    config.displacement_x_per_frame = 3;
    config.displacement_y_per_frame = -2;

    p2dic::SyntheticCamera camera(config);
    p2dic::Frame reference;
    p2dic::Frame deformed;

    camera.open();
    camera.start();
    if (!camera.grab(reference, 100ms) || !camera.grab(deformed, 100ms)) {
        std::cerr << "Failed to acquire synthetic frames\n";
        return 1;
    }

    const auto start = std::chrono::steady_clock::now();
    const auto result = p2dic::find_integer_translation_zncc(
        reference, deformed, 256, 256, 20, 8);
    const auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start);

    camera.stop();
    camera.close();

    std::cout << "Camera: " << camera.name() << '\n'
              << "Recovered translation: dx=" << result.dx << ", dy=" << result.dy << '\n'
              << "ZNCC: " << std::fixed << std::setprecision(6) << result.score << '\n'
              << "CPU time: " << std::setprecision(3) << elapsed.count() << " ms\n";

    return result.dx == 3 && result.dy == -2 ? 0 : 2;
}
