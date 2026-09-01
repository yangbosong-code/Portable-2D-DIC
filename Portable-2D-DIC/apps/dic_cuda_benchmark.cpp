#include "p2dic/cuda_grid_engine.hpp"
#include "p2dic/synthetic_camera.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

int positive_argument(char* value, std::string_view name) {
    const int parsed = std::atoi(value);
    if (parsed <= 0) {
        throw std::invalid_argument(std::string(name) + " must be positive");
    }
    return parsed;
}

double percentile(std::vector<double> values, double fraction) {
    std::sort(values.begin(), values.end());
    const auto index = static_cast<std::size_t>(
        std::ceil(fraction * static_cast<double>(values.size())) - 1.0);
    return values[std::min(index, values.size() - 1)];
}

}  // namespace

int main(int argc, char** argv) {
    try {
        using namespace std::chrono_literals;
        const int width = argc > 1 ? positive_argument(argv[1], "width") : 4504;
        const int height = argc > 2 ? positive_argument(argv[2], "height") : 4096;
        const int grid_step = argc > 3 ? positive_argument(argv[3], "grid_step") : 32;
        const int measured_frames = argc > 4 ? positive_argument(argv[4], "frames") : 20;
        const bool inverse_compositional = argc > 5 && std::string_view(argv[5]) == "ic";

        p2dic::SyntheticCameraConfig camera_config;
        camera_config.width = static_cast<std::uint32_t>(width);
        camera_config.height = static_cast<std::uint32_t>(height);
        camera_config.frames_per_second = 1000.0;
        camera_config.displacement_x_per_frame = 1;
        camera_config.displacement_y_per_frame = 0;
        p2dic::SyntheticCamera camera(camera_config);
        p2dic::Frame reference;
        p2dic::Frame deformed;
        camera.open();
        camera.start();
        if (!camera.grab(reference, 100ms) || !camera.grab(deformed, 100ms)) {
            throw std::runtime_error("Synthetic camera timed out");
        }
        p2dic::CudaGridConfig dic_config;
        dic_config.subset_radius = 15;
        dic_config.grid_step = grid_step;
        dic_config.max_iterations = 15;
        dic_config.convergence_tolerance = 1.0e-3F;
        dic_config.quality_threshold = 0.80F;
        dic_config.maximum_iteration_step = 1.0F;
        dic_config.inverse_compositional = inverse_compositional;
        p2dic::CudaGridEngine engine(dic_config);

        const auto cold = engine.process(reference, deformed);
        std::vector<double> durations;
        durations.reserve(static_cast<std::size_t>(measured_frames));
        std::size_t valid = 0;
        std::size_t points = 0;
        p2dic::DicResult last_result;
        p2dic::EngineStageTiming stage_sum;
        for (int frame = 0; frame < measured_frames; ++frame) {
            if (!camera.grab(deformed, 100ms)) {
                throw std::runtime_error("Synthetic camera timed out during benchmark");
            }
            const auto result = engine.process(reference, deformed);
            durations.push_back(result.processing_ms);
            points = result.points.size();
            valid = static_cast<std::size_t>(std::count_if(
                result.points.begin(), result.points.end(),
                [](const p2dic::DicPoint& point) { return point.valid; }));
            last_result = result;
            const auto stage = engine.last_stage_timing();
            stage_sum.host_staging_ms += stage.host_staging_ms;
            stage_sum.h2d_ms += stage.h2d_ms;
            stage_sum.kernel_ms += stage.kernel_ms;
            stage_sum.d2h_ms += stage.d2h_ms;
            stage_sum.kernel_launches += stage.kernel_launches;
        }
        camera.stop();
        camera.close();

        const double mean = std::accumulate(durations.begin(), durations.end(), 0.0) /
                            static_cast<double>(durations.size());
        const double p50 = percentile(durations, 0.50);
        const double p95 = percentile(durations, 0.95);
        double mean_u = 0.0;
        double mean_v = 0.0;
        double mean_quality = 0.0;
        double mean_absolute_u_error = 0.0;
        const double expected_u = static_cast<double>(measured_frames + 1);
        for (const auto& point : last_result.points) {
            mean_u += point.u;
            mean_v += point.v;
            mean_quality += point.quality;
            mean_absolute_u_error += std::abs(static_cast<double>(point.u) - expected_u);
        }
        if (points != 0) {
            mean_u /= static_cast<double>(points);
            mean_v /= static_cast<double>(points);
            mean_quality /= static_cast<double>(points);
            mean_absolute_u_error /= static_cast<double>(points);
        }
        std::cout << std::fixed << std::setprecision(3)
                  << "image=" << width << 'x' << height << '\n'
                  << "grid_step=" << grid_step << " points=" << points << '\n'
                  << "solver=" << (inverse_compositional ? "IC-GN" : "FA-GN") << '\n'
                  << "motion=1_px_per_frame measured_frames=" << measured_frames << '\n'
                  << "valid_ratio="
                  << (points == 0 ? 0.0 : static_cast<double>(valid) / points) << '\n'
                  << "expected_u=" << expected_u << " mean_u=" << mean_u
                  << " mean_v=" << mean_v << '\n'
                  << "mean_abs_u_error_all=" << mean_absolute_u_error
                  << " mean_quality_all=" << mean_quality << '\n'
                  << "cold_ms=" << cold.processing_ms << '\n'
                  << "steady_mean_ms=" << mean << '\n'
                  << "steady_p50_ms=" << p50 << '\n'
                  << "steady_p95_ms=" << p95 << '\n'
                  << "stage_mean_staging_ms=" << stage_sum.host_staging_ms / measured_frames << '\n'
                  << "stage_mean_h2d_ms=" << stage_sum.h2d_ms / measured_frames << '\n'
                  << "stage_mean_kernel_ms=" << stage_sum.kernel_ms / measured_frames << '\n'
                  << "stage_mean_d2h_ms=" << stage_sum.d2h_ms / measured_frames << '\n'
                  << "kernel_launches=" << stage_sum.kernel_launches << '\n'
                  << "estimated_compute_fps=" << (mean > 0.0 ? 1000.0 / mean : 0.0) << '\n';
        return valid == 0 ? 2 : 0;
    } catch (const std::exception& error) {
        std::cerr << "CUDA benchmark failed: " << error.what() << '\n';
        return 1;
    }
}
