#include "p2dic/cuda_grid_engine.hpp"
#include "p2dic/synthetic_camera.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    using namespace std::chrono_literals;
    p2dic::SyntheticCameraConfig camera_config;
    camera_config.width = 1024;
    camera_config.height = 1024;
    camera_config.frames_per_second = 1000.0;
    camera_config.displacement_x_per_frame = 1;
    camera_config.displacement_y_per_frame = 0;
    p2dic::SyntheticCamera camera(camera_config);
    p2dic::Frame reference;
    p2dic::Frame deformed;
    camera.open();
    camera.start();
    if (!camera.grab(reference, 100ms)) {
        std::cerr << "Synthetic camera timed out\n";
        return 1;
    }
    if (!reference.pixels.pinned()) {
        std::cerr << "CUDA frame buffer did not use page-locked memory\n";
        return 3;
    }

    p2dic::CudaGridConfig engine_config{10, 32, 25, 1.0e-3F, 0.80F, 1.0F};
    engine_config.inverse_compositional = argc > 1 && std::string_view(argv[1]) == "ic";
    p2dic::CudaGridEngine engine(engine_config);
    p2dic::DicResult result;
    constexpr int tracked_frames = 20;
    for (int expected = 1; expected <= tracked_frames; ++expected) {
        if (!camera.grab(deformed, 100ms)) {
            std::cerr << "Synthetic camera timed out while tracking\n";
            return 1;
        }
        result = engine.process(reference, deformed);
    }
    std::size_t valid = 0;
    double absolute_error = 0.0;
    for (const auto& point : result.points) {
        if (point.valid) {
            ++valid;
            absolute_error += std::abs(point.u - static_cast<float>(tracked_frames)) +
                              std::abs(point.v);
        }
    }
    const double valid_ratio = result.points.empty()
                                   ? 0.0
                                   : static_cast<double>(valid) / result.points.size();
    const double mean_error = valid == 0 ? 999.0 : absolute_error / valid;
    std::cout << "solver=" << engine.name() << " points=" << result.points.size()
              << " valid_ratio=" << valid_ratio
              << " mean_l1_error=" << mean_error
              << " processing_ms=" << result.processing_ms << '\n';
    if (valid_ratio < 0.95 || mean_error > 0.08) {
        std::cerr << "CUDA grid accuracy threshold failed\n";
        return 2;
    }
    return 0;
}
