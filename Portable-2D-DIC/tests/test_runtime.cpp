#include "p2dic/frame_pool.hpp"
#include "p2dic/latest_queue.hpp"
#include "p2dic/pipeline.hpp"
#include "p2dic/synthetic_camera.hpp"
#include "p2dic/zncc_grid_engine.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

namespace {

bool test_latest_queue() {
    p2dic::LatestQueue<int> queue(2);
    if (queue.push(1) != 0 || queue.push(2) != 0 || queue.push(3) != 1) {
        return false;
    }
    const auto value = queue.wait_pop(std::chrono::milliseconds(10));
    return value && value->value == 3 && value->skipped == 1;
}

bool test_frame_pool() {
    p2dic::FramePool pool(2, 64, 64);
    auto a = pool.acquire(std::chrono::milliseconds(10));
    auto b = pool.acquire(std::chrono::milliseconds(10));
    auto none = pool.acquire(std::chrono::milliseconds(2));
    if (!a || !b || none) {
        return false;
    }
    a.reset();
    return static_cast<bool>(pool.acquire(std::chrono::milliseconds(10)));
}

bool test_pipeline() {
    p2dic::RuntimeConfig runtime;
    runtime.width = 128;
    runtime.height = 128;
    runtime.camera_fps = 200.0;
    runtime.subset_radius = 6;
    runtime.grid_step = 64;
    runtime.search_radius = 4;
    runtime.frame_pool_size = 7;
    runtime.capture_queue_size = 2;

    p2dic::SyntheticCameraConfig camera_config;
    camera_config.width = runtime.width;
    camera_config.height = runtime.height;
    camera_config.frames_per_second = runtime.camera_fps;
    camera_config.displacement_x_per_frame = 0;

    p2dic::ZnccGridConfig engine_config;
    engine_config.subset_radius = runtime.subset_radius;
    engine_config.grid_step = runtime.grid_step;
    engine_config.search_radius = runtime.search_radius;

    p2dic::ProcessingPipeline pipeline(
        runtime,
        std::make_unique<p2dic::SyntheticCamera>(camera_config),
        std::make_unique<p2dic::ZnccGridEngine>(engine_config));
    pipeline.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    pipeline.pause();
    const auto paused_metrics = pipeline.metrics();
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    const auto still_paused_metrics = pipeline.metrics();
    if (pipeline.state() != p2dic::PipelineState::paused ||
        still_paused_metrics.processed != paused_metrics.processed ||
        still_paused_metrics.captured <= paused_metrics.captured) {
        return false;
    }
    pipeline.resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    pipeline.stop();
    const auto first_metrics = pipeline.metrics();
    if (pipeline.state() != p2dic::PipelineState::stopped ||
        first_metrics.captured <= 2 ||
        first_metrics.processed <= still_paused_metrics.processed ||
        first_metrics.capture.sample_count == 0 || first_metrics.dic.sample_count == 0 ||
        first_metrics.pipeline.sample_count != first_metrics.processed ||
        first_metrics.pipeline.p95 <= 0.0 || first_metrics.last_pipeline_ms <= 0.0) {
        return false;
    }

    // A production experiment can be started and stopped repeatedly without
    // reconstructing the whole application.
    pipeline.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    pipeline.stop();
    const auto second_metrics = pipeline.metrics();
    return pipeline.state() == p2dic::PipelineState::stopped &&
           second_metrics.captured > 2 && second_metrics.processed > 0 &&
           second_metrics.pipeline.sample_count == second_metrics.processed;
}

}  // namespace

int main() {
    if (!test_latest_queue()) {
        std::cerr << "LatestQueue test failed\n";
        return 1;
    }
    if (!test_frame_pool()) {
        std::cerr << "FramePool test failed\n";
        return 2;
    }
    if (!test_pipeline()) {
        std::cerr << "ProcessingPipeline test failed\n";
        return 3;
    }
    return 0;
}
