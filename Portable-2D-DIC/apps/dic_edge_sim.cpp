#include "p2dic/pipeline.hpp"
#include "p2dic/synthetic_camera.hpp"
#include "p2dic/zncc_grid_engine.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

int main() {
    p2dic::RuntimeConfig runtime;
    runtime.width = 256;
    runtime.height = 256;
    runtime.camera_fps = 30.0;
    runtime.subset_radius = 12;
    runtime.grid_step = 64;
    runtime.search_radius = 6;
    runtime.frame_pool_size = 8;
    runtime.capture_queue_size = 3;

    p2dic::SyntheticCameraConfig camera_config;
    camera_config.width = runtime.width;
    camera_config.height = runtime.height;
    camera_config.frames_per_second = runtime.camera_fps;
    camera_config.displacement_x_per_frame = 1;
    camera_config.displacement_y_per_frame = 0;

    p2dic::ZnccGridConfig dic_config;
    dic_config.subset_radius = runtime.subset_radius;
    dic_config.grid_step = runtime.grid_step;
    dic_config.search_radius = runtime.search_radius;

    p2dic::ProcessingPipeline pipeline(
        runtime,
        std::make_unique<p2dic::SyntheticCamera>(camera_config),
        std::make_unique<p2dic::ZnccGridEngine>(dic_config));

    pipeline.set_result_callback([](const p2dic::DicResult& result) {
        std::cout << "frame=" << result.frame_sequence
                  << " points=" << result.points.size()
                  << " processing_ms=" << result.processing_ms << '\n';
    });
    pipeline.set_fault_callback([](const std::string& message) {
        std::cerr << message << '\n';
    });

    pipeline.start();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    pipeline.stop();

    const auto metrics = pipeline.metrics();
    std::cout << "captured=" << metrics.captured
              << " processed=" << metrics.processed
              << " dropped=" << metrics.dropped
              << " timeouts=" << metrics.capture_timeouts << '\n';
    return metrics.processed > 0 ? 0 : 1;
}
