#pragma once

#include "p2dic/camera.hpp"
#include "p2dic/dic_engine.hpp"
#include "p2dic/frame_pool.hpp"
#include "p2dic/latest_queue.hpp"
#include "p2dic/performance_stats.hpp"
#include "p2dic/runtime_config.hpp"
#include "p2dic/strain_estimator.hpp"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace p2dic {

enum class PipelineState {
    stopped,
    starting,
    running,
    paused,
    stopping,
    faulted,
};

struct PipelineMetricsSnapshot {
    std::uint64_t captured{};
    std::uint64_t processed{};
    std::uint64_t dropped{};
    std::uint64_t capture_timeouts{};
    double last_processing_ms{};
    double last_capture_ms{};
    double last_dic_ms{};
    double last_strain_ms{};
    double last_callback_ms{};
    double last_pipeline_ms{};
    double last_end_to_end_ms{};
    double last_gpu_staging_ms{};
    double last_h2d_ms{};
    double last_kernel_ms{};
    double last_d2h_ms{};
    int last_kernel_launches{};
    DistributionSnapshot capture;
    DistributionSnapshot dic;
    DistributionSnapshot strain;
    DistributionSnapshot callback;
    DistributionSnapshot pipeline;
    DistributionSnapshot end_to_end;
    DistributionSnapshot h2d;
    DistributionSnapshot kernel;
    DistributionSnapshot d2h;
};

class ProcessingPipeline {
public:
    using ResultCallback = std::function<void(const DicResult&)>;
    using PreviewCallback = std::function<void(const Frame&)>;
    using FaultCallback = std::function<void(const std::string&)>;

    ProcessingPipeline(
        RuntimeConfig config,
        std::unique_ptr<ICamera> camera,
        std::unique_ptr<IDicEngine> engine);
    ~ProcessingPipeline();

    ProcessingPipeline(const ProcessingPipeline&) = delete;
    ProcessingPipeline& operator=(const ProcessingPipeline&) = delete;

    void start();
    void pause();
    void resume();
    void stop() noexcept;
    [[nodiscard]] PipelineState state() const noexcept;
    [[nodiscard]] PipelineMetricsSnapshot metrics() const noexcept;
    void set_result_callback(ResultCallback callback);
    void set_preview_callback(PreviewCallback callback);
    void set_fault_callback(FaultCallback callback);

private:
    void capture_loop();
    void processing_loop();
    void report_fault(const std::string& message) noexcept;

    RuntimeConfig config_;
    std::unique_ptr<ICamera> camera_;
    std::unique_ptr<IDicEngine> engine_;
    GridStrainEstimator strain_estimator_;
    FramePool frame_pool_;
    LatestQueue<std::shared_ptr<Frame>> capture_queue_;
    std::shared_ptr<Frame> reference_;
    std::thread capture_thread_;
    std::thread processing_thread_;
    std::atomic<PipelineState> state_{PipelineState::stopped};
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> paused_{false};
    std::atomic<bool> processing_active_{false};
    std::atomic<std::uint64_t> captured_{0};
    std::atomic<std::uint64_t> processed_{0};
    std::atomic<std::uint64_t> dropped_{0};
    std::atomic<std::uint64_t> capture_timeouts_{0};
    std::atomic<double> last_processing_ms_{0.0};
    std::atomic<double> last_capture_ms_{0.0};
    std::atomic<double> last_dic_ms_{0.0};
    std::atomic<double> last_strain_ms_{0.0};
    std::atomic<double> last_callback_ms_{0.0};
    std::atomic<double> last_pipeline_ms_{0.0};
    std::atomic<double> last_end_to_end_ms_{0.0};
    std::atomic<double> last_gpu_staging_ms_{0.0};
    std::atomic<double> last_h2d_ms_{0.0};
    std::atomic<double> last_kernel_ms_{0.0};
    std::atomic<double> last_d2h_ms_{0.0};
    std::atomic<int> last_kernel_launches_{0};
    RollingStatistics capture_statistics_;
    RollingStatistics dic_statistics_;
    RollingStatistics strain_statistics_;
    RollingStatistics callback_statistics_;
    RollingStatistics pipeline_statistics_;
    RollingStatistics end_to_end_statistics_;
    RollingStatistics h2d_statistics_;
    RollingStatistics kernel_statistics_;
    RollingStatistics d2h_statistics_;
    mutable std::mutex callback_mutex_;
    mutable std::mutex pause_mutex_;
    std::condition_variable pause_condition_;
    ResultCallback result_callback_;
    PreviewCallback preview_callback_;
    FaultCallback fault_callback_;
};

}  // namespace p2dic
