#include "p2dic/subpixel.hpp"
#include "p2dic/synthetic_camera.hpp"
#include "p2dic/zncc.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>

namespace {

double sample(const p2dic::Frame& frame, double x, double y) {
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    if (x0 < 0 || y0 < 0 || x0 + 1 >= static_cast<int>(frame.width) ||
        y0 + 1 >= static_cast<int>(frame.height)) {
        return 225.0;
    }
    const double tx = x - x0;
    const double ty = y - y0;
    const auto at = [&frame](int px, int py) {
        return static_cast<double>(frame.pixels[static_cast<std::size_t>(py) * frame.stride + px]);
    };
    return (1.0 - ty) * ((1.0 - tx) * at(x0, y0) + tx * at(x0 + 1, y0)) +
           ty * ((1.0 - tx) * at(x0, y0 + 1) + tx * at(x0 + 1, y0 + 1));
}

p2dic::Frame shifted(const p2dic::Frame& reference, double u, double v) {
    p2dic::Frame output(reference.width, reference.height);
    for (int y = 0; y < static_cast<int>(output.height); ++y) {
        for (int x = 0; x < static_cast<int>(output.width); ++x) {
            const double value = sample(reference, x - u, y - v);
            output.pixels[static_cast<std::size_t>(y) * output.stride + x] =
                static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L));
        }
    }
    return output;
}

p2dic::Frame box_blurred(const p2dic::Frame& input, int passes) {
    p2dic::Frame current = input;
    for (int pass = 0; pass < passes; ++pass) {
        p2dic::Frame next = current;
        for (int y = 1; y + 1 < static_cast<int>(current.height); ++y) {
            for (int x = 1; x + 1 < static_cast<int>(current.width); ++x) {
                int sum = 0;
                for (int ky = -1; ky <= 1; ++ky) {
                    for (int kx = -1; kx <= 1; ++kx) {
                        sum += current.pixels[
                            static_cast<std::size_t>(y + ky) * current.stride + x + kx];
                    }
                }
                next.pixels[static_cast<std::size_t>(y) * next.stride + x] =
                    static_cast<std::uint8_t>((sum + 4) / 9);
            }
        }
        current = std::move(next);
    }
    return current;
}

}  // namespace

int main() {
    using namespace std::chrono_literals;
    p2dic::SyntheticCameraConfig camera_config;
    camera_config.width = 512;
    camera_config.height = 512;
    camera_config.frames_per_second = 1000.0;
    camera_config.displacement_x_per_frame = 0;
    camera_config.displacement_y_per_frame = 0;

    p2dic::SyntheticCamera camera(camera_config);
    p2dic::Frame reference;
    camera.open();
    camera.start();
    if (!camera.grab(reference, 100ms)) {
        std::cerr << "Synthetic acquisition timed out\n";
        return 1;
    }

    constexpr double expected_u = 2.35;
    constexpr double expected_v = -1.70;
    reference = box_blurred(reference, 3);
    const auto deformed = shifted(reference, expected_u, expected_v);
    const auto integer = p2dic::find_integer_translation_zncc(
        reference, deformed, 256, 256, 24, 6);
    const auto refined = p2dic::refine_translation_znssd(
        reference,
        deformed,
        256,
        256,
        integer.dx,
        integer.dy,
        p2dic::SubpixelConfig{24, 40, 1.0e-5, 1.0});

    if (!refined.valid || !refined.converged ||
        std::abs(refined.u - expected_u) > 0.08 ||
        std::abs(refined.v - expected_v) > 0.08 || refined.zncc < 0.99) {
        std::cerr << "Expected approximately (" << expected_u << ", " << expected_v
                  << "), got (" << refined.u << ", " << refined.v << "), zncc="
                  << refined.zncc << ", iterations=" << refined.iterations << '\n';
        return 2;
    }
    return 0;
}
