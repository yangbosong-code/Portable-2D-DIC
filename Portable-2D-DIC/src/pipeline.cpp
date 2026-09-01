#include "p2dic/pipeline.hpp"

#include <chrono>
#include <exception>
#include <stdexcept>
#include <utility>

namespace p2dic {

ProcessingPipeline::ProcessingPipeline(
    RuntimeConfig config,
    std::unique_ptr<ICamera> camera,
    std::unique_ptr<IDicEngine> engine)
    : config_(config),
      camera_(std::move(camera)),
      engine_(std::move(engine)),
      strain_estimator_(config.grid_step),
      frame_pool_(config.frame_pool_size, config.width, config.height),
      capture_queue_(config.capture_queue_size) {
    config_.validate();
    if (!camera_ || !engine_) {
        throw std::invalid_argument("Pipeline camera and DIC engine are required");
    }
}

ProcessingPipeline::~ProcessingPipeline() {
    stop();
}

void ProcessingPipeline::start() {
    PipelineState expected = PipelineState::stopped;
    if (!state_.compare_exchange_strong(expected, PipelineState::starting)) {
        throw std::logic_error("Pipeline can only start from the stopped state");
    }

    stop_requested_ = false;
    paused_ = false;
    processing_active_ = false;
    try {
        frame_pool_.reopen();
        capture_queue_.reopen();
        reference_.reset();
        captured_ = 0;
        processed_ = 0;
        dropped_ = 0;
        capture_timeouts_ = 0;
        last_processing_ms_ = 0.0;
        last_capture_ms_ = 0.0;
        last_dic_ms_ = 0.0;
        last_strain_ms_ = 0.0;
        last_callback_ms_ = 0.0;
        last_pipeline_ms_ = 0.0;
        last_end_to_end_ms_ = 0.0;
        last_gpu_staging_ms_ = 0.0;
        last_h2d_ms_ = 0.0;
        last_kernel_ms_ = 0.0;
        last_d2h_ms_ = 0.0;
        last_kernel_launches_ = 0;
        capture_statistics_.clear();
        dic_statistics_.clear();
        strain_statistics_.clear();
        callback_statistics_.clear();
        pipeline_statistics_.clear();
        end_to_end_statistics_.clear();
        h2d_statistics_.clear();
        kernel_statistics_.clear();
        d2h_statistics_.clear();
        camera_->open();
        camera_->start();
        capture_thread_ = std::thread(&ProcessingPipeline::capture_loop, this);
        processing_thread_ = std::thread(&ProcessingPipeline::processing_loop, this);
        state_ = PipelineState::running;
    } catch (...) {
        stop_requested_ = true;
        try {
            camera_->stop();
            camera_->close();
        } catch (...) {
        }
        state_ = PipelineState::faulted;
        throw;
    }
}

void ProcessingPipeline::pause() {
    PipelineState expected = PipelineState::running;
    if (!state_.compare_exchange_strong(expected, PipelineState::paused)) {
        throw std::logic_error("Pipeline can only pause from the running state");
    }
    paused_ = true;
    std::unique_lock pause_lock(pause_mutex_);
    pause_condition_.wait(pause_lock, [&] { return !processing_active_.load(); });
}

void ProcessingPipeline::resume() {
    PipelineState expected = PipelineState::paused;
    if (!state_.compare_exchange_strong(expected, PipelineState::running)) {
        throw std::logic_error("Pipeline can only resume from the paused state");
    }
    paused_ = false;
    pause_condition_.notify_all();
}

void ProcessingPipeline::stop() noexcept {
    const auto current = state_.load();
    if (current == PipelineState::stopped) {
        return;
    }
    if (current != PipelineState::faulted) {
        state_ = PipelineState::stopping;
    }
    stop_requested_ = true;
    paused_ = false;
    pause_condition_.notify_all();
    capture_queue_.close();
    frame_pool_.close();

    try {
        camera_->stop();
    } catch (...) {
    }
    if (capture_thread_.joinable()) {
        capture_thread_.join();
    }
    if (processing_thread_.joinable()) {
        processing_thread_.join();
    }
    try {
        camera_->close();
    } catch (...) {
    }
    state_ = PipelineState::stopped;
}

PipelineState ProcessingPipeline::state() const noexcept {
    return state_.load();
}

PipelineMetricsSnapshot ProcessingPipeline::metrics() const noexcept {
    return PipelineMetricsSnapshot{
        captured_.load(),
        processed_.load(),
        dropped_.load(),
        capture_timeouts_.load(),
        last_processing_ms_.load(),
        last_capture_ms_.load(),
        last_dic_ms_.load(),
        last_strain_ms_.load(),
        last_callback_ms_.load(),
        last_pipeline_ms_.load(),
        last_end_to_end_ms_.load(),
        last_gpu_staging_ms_.load(),
        last_h2d_ms_.load(),
        last_kernel_ms_.load(),
        last_d2h_ms_.load(),
        last_kernel_launches_.load(),
        capture_statistics_.snapshot(),
        dic_statistics_.snapshot(),
        strain_statistics_.snapshot(),
        callback_statistics_.snapshot(),
        pipeline_statistics_.snapshot(),
        end_to_end_statistics_.snapshot(),
        h2d_statistics_.snapshot(),
        kernel_statistics_.snapshot(),
        d2h_statistics_.snapshot()};
}

void ProcessingPipeline::set_result_callback(ResultCallback callback) {
    std::lock_guard lock(callback_mutex_);
    result_callback_ = std::move(callback);
}

void ProcessingPipeline::set_preview_callback(PreviewCallback callback) {
    std::lock_guard lock(callback_mutex_);
    preview_callback_ = std::move(callback);
}

void ProcessingPipeline::set_fault_callback(FaultCallback callback) {
    std::lock_guard lock(callback_mutex_);
    fault_callback_ = std::move(callback);
}

void ProcessingPipeline::capture_loop() {
    using namespace std::chrono_literals;
    try {
        while (!stop_requested_) {
            auto frame = frame_pool_.acquire(100ms);
            if (!frame) {
                continue;
            }
            const auto capture_start = std::chrono::steady_clock::now();
            const bool grabbed = camera_->grab(*frame, 1000ms);
            const double capture_ms = std::chrono::duration<double, std::milli>(
                                          std::chrono::steady_clock::now() - capture_start)
                                          .count();
            last_capture_ms_ = capture_ms;
            capture_statistics_.add(capture_ms);
            if (!grabbed) {
                ++capture_timeouts_;
                continue;
            }
            ++captured_;
            dropped_ += capture_queue_.push(std::move(frame));
        }
    } catch (const std::exception& error) {
        report_fault(std::string("Capture loop failed: ") + error.what());
    } catch (...) {
        report_fault("Capture loop failed with an unknown error");
    }
}

void ProcessingPipeline::processing_loop() {
    using namespace std::chrono_literals;
    try {
        while (!stop_requested_) {
            auto item = capture_queue_.wait_pop(200ms);
            if (!item) {
                continue;
            }
            {
                std::unique_lock pause_lock(pause_mutex_);
                pause_condition_.wait(pause_lock, [&] {
                    return stop_requested_.load() || !paused_.load();
                });
                if (stop_requested_) break;
                processing_active_ = true;
            }
            dropped_ += item->skipped;
            auto frame = std::move(item->value);
            if (!reference_) {
                reference_ = std::make_shared<Frame>(*frame);
                processing_active_ = false;
                pause_condition_.notify_all();
                continue;
            }

            const auto processing_start = std::chrono::steady_clock::now();
            const auto dic_start = processing_start;
            auto result = engine_->process(*reference_, *frame);
            const auto engine_timing = engine_->last_stage_timing();
            const auto dic_end = std::chrono::steady_clock::now();
            strain_estimator_.apply(result);
            const auto strain_end = std::chrono::steady_clock::now();
            const double dic_ms = std::chrono::duration<double, std::milli>(
                                      dic_end - dic_start).count();
            const double strain_ms = std::chrono::duration<double, std::milli>(
                                         strain_end - dic_end).count();
            result.processing_ms = dic_ms + strain_ms;
            last_processing_ms_ = result.processing_ms;
            last_dic_ms_ = dic_ms;
            last_strain_ms_ = strain_ms;
            last_gpu_staging_ms_ = engine_timing.host_staging_ms;
            last_h2d_ms_ = engine_timing.h2d_ms;
            last_kernel_ms_ = engine_timing.kernel_ms;
            last_d2h_ms_ = engine_timing.d2h_ms;
            last_kernel_launches_ = engine_timing.kernel_launches;
            dic_statistics_.add(dic_ms);
            strain_statistics_.add(strain_ms);
            if (engine_timing.kernel_launches > 0) {
                h2d_statistics_.add(engine_timing.h2d_ms);
                kernel_statistics_.add(engine_timing.kernel_ms);
                d2h_statistics_.add(engine_timing.d2h_ms);
            }
            ++processed_;

            ResultCallback callback;
            PreviewCallback preview_callback;
            {
                std::lock_guard lock(callback_mutex_);
                callback = result_callback_;
                preview_callback = preview_callback_;
            }
            const auto callback_start = std::chrono::steady_clock::now();
            if (callback) {
                callback(result);
            }
            if (preview_callback) {
                preview_callback(*frame);
            }
            const auto processing_end = std::chrono::steady_clock::now();
            const double callback_ms = std::chrono::duration<double, std::milli>(
                                           processing_end - callback_start).count();
            const double pipeline_ms = std::chrono::duration<double, std::milli>(
                                           processing_end - processing_start).count();
            const auto now_ns = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    processing_end.time_since_epoch()).count());
            const double end_to_end_ms = frame->timestamp_ns <= now_ns
                ? static_cast<double>(now_ns - frame->timestamp_ns) / 1.0e6
                : 0.0;
            last_callback_ms_ = callback_ms;
            last_pipeline_ms_ = pipeline_ms;
            last_end_to_end_ms_ = end_to_end_ms;
            callback_statistics_.add(callback_ms);
            pipeline_statistics_.add(pipeline_ms);
            end_to_end_statistics_.add(end_to_end_ms);
            processing_active_ = false;
            pause_condition_.notify_all();
        }
    } catch (const std::exception& error) {
        processing_active_ = false;
        pause_condition_.notify_all();
        report_fault(std::string("Processing loop failed: ") + error.what());
    } catch (...) {
        processing_active_ = false;
        pause_condition_.notify_all();
        report_fault("Processing loop failed with an unknown error");
    }
}

void ProcessingPipeline::report_fault(const std::string& message) noexcept {
    state_ = PipelineState::faulted;
    stop_requested_ = true;
    FaultCallback callback;
    {
        std::lock_guard lock(callback_mutex_);
        callback = fault_callback_;
    }
    if (callback) {
        try {
            callback(message);
        } catch (...) {
        }
    }
}

}  // namespace p2dic
