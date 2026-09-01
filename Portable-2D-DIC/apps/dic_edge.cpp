#include "p2dic/control_protocol.hpp"
#include "p2dic/async_result_writer.hpp"
#include "p2dic/edge_config.hpp"
#include "p2dic/edge_state.hpp"
#include "p2dic/galaxy_camera.hpp"
#include "p2dic/hybrid_grid_engine.hpp"
#include "p2dic/pipeline.hpp"
#include "p2dic/preview_frame.hpp"
#include "p2dic/replay_camera.hpp"
#include "p2dic/result_summary.hpp"
#include "p2dic/result_stream_client.hpp"
#include "p2dic/result_stream_server.hpp"
#include "p2dic/session_binary.hpp"
#include "p2dic/synthetic_camera.hpp"
#include "p2dic/tcp_control_server.hpp"
#include "p2dic/tcp_control_client.hpp"

#if defined(P2DIC_WITH_CUDA)
#include "p2dic/cuda_grid_engine.hpp"
#endif

#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

volatile std::sig_atomic_t signal_requested = 0;

void handle_signal(int) {
    signal_requested = 1;
}

std::string argument_value(const p2dic::ControlCommand& command, std::string_view name) {
    const std::string prefix = std::string(name) + '=';
    for (const auto& argument : command.arguments) {
        if (argument.starts_with(prefix)) {
            return argument.substr(prefix.size());
        }
    }
    return {};
}

double parse_double_setting(std::string_view value, std::string_view key) {
    std::size_t parsed = 0;
    const std::string text(value);
    double output = 0.0;
    try {
        output = std::stod(text, &parsed);
    } catch (...) {
        throw std::invalid_argument(std::string(key) + " is not a number");
    }
    if (parsed != text.size() || !std::isfinite(output)) {
        throw std::invalid_argument(std::string(key) + " is not a finite number");
    }
    return output;
}

std::uint32_t parse_u32_setting(std::string_view value, std::string_view key) {
    std::size_t parsed = 0;
    const std::string text(value);
    unsigned long output = 0;
    try {
        output = std::stoul(text, &parsed);
    } catch (...) {
        throw std::invalid_argument(std::string(key) + " is not an unsigned integer");
    }
    if (parsed != text.size() || output > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument(std::string(key) + " is outside uint32 range");
    }
    return static_cast<std::uint32_t>(output);
}

int parse_int_setting(std::string_view value, std::string_view key) {
    std::size_t parsed = 0;
    const std::string text(value);
    long output = 0;
    try {
        output = std::stol(text, &parsed);
    } catch (...) {
        throw std::invalid_argument(std::string(key) + " is not an integer");
    }
    if (parsed != text.size() || output < std::numeric_limits<int>::min() ||
        output > std::numeric_limits<int>::max()) {
        throw std::invalid_argument(std::string(key) + " is outside int range");
    }
    return static_cast<int>(output);
}

bool parse_bool_setting(std::string_view value, std::string_view key) {
    if (value == "true" || value == "1") return true;
    if (value == "false" || value == "0") return false;
    throw std::invalid_argument(std::string(key) + " expects true or false");
}

void apply_runtime_setting(p2dic::EdgeConfig& config, std::string_view argument) {
    const auto equal = argument.find('=');
    if (equal == std::string_view::npos || equal == 0 || equal + 1 >= argument.size()) {
        throw std::invalid_argument("CONFIG arguments must use key=value");
    }
    const auto key = argument.substr(0, equal);
    const auto value = argument.substr(equal + 1);
    if (key == "camera.exposure_us") config.galaxy.exposure_us = parse_double_setting(value, key);
    else if (key == "camera.gain_db") config.galaxy.gain_db = parse_double_setting(value, key);
    else if (key == "camera.external_trigger") config.galaxy.external_trigger = parse_bool_setting(value, key);
    else if (key == "camera.offset_x") config.galaxy.offset_x = parse_u32_setting(value, key);
    else if (key == "camera.offset_y") config.galaxy.offset_y = parse_u32_setting(value, key);
    else if (key == "image.width") config.runtime.width = config.galaxy.width = parse_u32_setting(value, key);
    else if (key == "image.height") config.runtime.height = config.galaxy.height = parse_u32_setting(value, key);
    else if (key == "image.fps") config.runtime.camera_fps = parse_double_setting(value, key);
    else if (key == "dic.subset_radius") config.runtime.subset_radius = parse_int_setting(value, key);
    else if (key == "dic.grid_step") config.runtime.grid_step = parse_int_setting(value, key);
    else if (key == "dic.search_radius") config.runtime.search_radius = parse_int_setting(value, key);
    else if (key == "dic.quality_threshold") config.quality_threshold = parse_double_setting(value, key);
    else throw std::invalid_argument("CONFIG key is not runtime configurable: " + std::string(key));
}

std::string configuration_fields(const p2dic::EdgeConfig& config) {
    std::ostringstream fields;
    fields << std::setprecision(10)
           << "camera.backend=" << p2dic::to_string(config.camera_backend)
           << " dic.backend=" << p2dic::to_string(config.dic_backend)
           << " camera.exposure_us=" << config.galaxy.exposure_us
           << " camera.gain_db=" << config.galaxy.gain_db
           << " camera.external_trigger=" << (config.galaxy.external_trigger ? "true" : "false")
           << " camera.offset_x=" << config.galaxy.offset_x
           << " camera.offset_y=" << config.galaxy.offset_y
           << " image.width=" << config.runtime.width
           << " image.height=" << config.runtime.height
           << " image.fps=" << config.runtime.camera_fps
           << " dic.subset_radius=" << config.runtime.subset_radius
           << " dic.grid_step=" << config.runtime.grid_step
           << " dic.search_radius=" << config.runtime.search_radius
           << " dic.quality_threshold=" << config.quality_threshold;
    fields << " dic.inverse_compositional="
           << (config.inverse_compositional ? "true" : "false");
    return fields.str();
}

std::unique_ptr<p2dic::ICamera> make_camera(const p2dic::EdgeConfig& config) {
    switch (config.camera_backend) {
        case p2dic::CameraBackend::synthetic: {
            p2dic::SyntheticCameraConfig camera_config;
            camera_config.width = config.runtime.width;
            camera_config.height = config.runtime.height;
            camera_config.frames_per_second = config.runtime.camera_fps;
            camera_config.seed = config.synthetic_seed;
            camera_config.displacement_x_per_frame = config.synthetic_dx_per_frame;
            camera_config.displacement_y_per_frame = config.synthetic_dy_per_frame;
            camera_config.motion_mode = config.synthetic_motion_mode;
            camera_config.displacement_x_amplitude = config.synthetic_displacement_x_amplitude;
            camera_config.displacement_y_amplitude = config.synthetic_displacement_y_amplitude;
            camera_config.motion_frequency_hz = config.synthetic_motion_frequency_hz;
            return std::make_unique<p2dic::SyntheticCamera>(camera_config);
        }
        case p2dic::CameraBackend::replay:
            return std::make_unique<p2dic::ReplayCamera>(p2dic::ReplayCameraConfig{
                config.replay_directory, config.runtime.camera_fps, config.replay_loop});
        case p2dic::CameraBackend::galaxy:
#if defined(P2DIC_WITH_GALAXY)
            return std::make_unique<p2dic::GalaxyCamera>(config.galaxy);
#else
            throw std::runtime_error("Galaxy backend was not compiled");
#endif
    }
    throw std::runtime_error("Unknown camera backend");
}

std::unique_ptr<p2dic::IDicEngine> make_engine(const p2dic::EdgeConfig& config) {
    switch (config.dic_backend) {
        case p2dic::DicBackend::cpu:
            return std::make_unique<p2dic::HybridGridEngine>(p2dic::HybridGridConfig{
                config.runtime.subset_radius, config.runtime.grid_step,
                config.runtime.search_radius, config.max_iterations,
                config.convergence_tolerance, config.quality_threshold});
        case p2dic::DicBackend::cuda:
#if defined(P2DIC_WITH_CUDA)
            return std::make_unique<p2dic::CudaGridEngine>(p2dic::CudaGridConfig{
                config.runtime.subset_radius, config.runtime.grid_step,
                config.max_iterations, static_cast<float>(config.convergence_tolerance),
                static_cast<float>(config.quality_threshold),
                static_cast<float>(config.maximum_iteration_step),
                static_cast<float>(config.recovery_trigger_valid_ratio),
                config.max_recovery_passes,
                config.inverse_compositional});
#else
            throw std::runtime_error("CUDA backend was not compiled");
#endif
    }
    throw std::runtime_error("Unknown DIC backend");
}

std::unique_ptr<p2dic::ProcessingPipeline> make_pipeline(const p2dic::EdgeConfig& config) {
    config.validate();
    return std::make_unique<p2dic::ProcessingPipeline>(
        config.runtime, make_camera(config), make_engine(config));
}

}  // namespace

int main(int argc, char** argv) {
    const bool self_test = argc >= 2 && std::string_view(argv[1]) == "--self-test";
    p2dic::EdgeConfig config;
    std::filesystem::path config_path;
    config.runtime.width = 512;
    config.runtime.height = 512;
    config.runtime.camera_fps = 20.0;
    config.runtime.subset_radius = 18;
    config.runtime.grid_step = 64;
    config.runtime.search_radius = 6;
    if (self_test) {
        config.storage_root = argc >= 3 ? argv[2] : "self-test-sessions";
        config.control_port = static_cast<std::uint16_t>(argc >= 4 ? std::stoul(argv[3]) : 17840);
        config.result_port = static_cast<std::uint16_t>(config.control_port + 1);
        config.preview_port = static_cast<std::uint16_t>(config.control_port + 2);
        config.bind_address = "127.0.0.1";
    } else if (argc >= 2 && std::string_view(argv[1]) == "--config") {
        if (argc < 3) {
            std::cerr << "Usage: dic_edge --config PATH\n";
            return 2;
        }
        try {
            config_path = argv[2];
            config = p2dic::load_edge_config(config_path);
        } catch (const std::exception& exception) {
            std::cerr << "ERROR config message=\"" << exception.what() << "\"\n";
            return 2;
        }
    } else if (argc > 1) {
        std::cerr << "Usage: dic_edge [--config PATH] | [--self-test DATA_ROOT PORT]\n";
        return 2;
    }

    std::unique_ptr<p2dic::ProcessingPipeline> pipeline;
    try {
        pipeline = make_pipeline(config);
    } catch (const std::exception& exception) {
        std::cerr << "ERROR pipeline_create message=\"" << exception.what() << "\"\n";
        return 2;
    }
    const std::string camera_name =
        config.camera_backend == p2dic::CameraBackend::galaxy ? "DAHENG-GalaxySDK" :
        config.camera_backend == p2dic::CameraBackend::replay ? "PGM-replay" : "synthetic";
    p2dic::AsyncResultWriter writer(p2dic::AsyncWriterConfig{
        config.storage_queue_capacity,
        config.storage_flush_every_frames,
        config.storage_csv_enabled});
    p2dic::ResultStreamServer result_server;
    p2dic::ResultStreamServer preview_server;
    p2dic::EdgeStateMachine state;
    std::mutex command_mutex;
    std::mutex summary_mutex;
    p2dic::ResultSummary latest_summary;
    bool summary_available = false;
    std::atomic<bool> shutdown_requested{false};
    std::atomic<bool> self_test_failed{false};
    std::uint64_t last_preview_timestamp_ns = 0;

    const auto attach_pipeline_callbacks = [&] {
        pipeline->set_result_callback([&](const p2dic::DicResult& result) {
            if (!writer.enqueue(result)) {
                throw std::runtime_error("Storage queue is full; refusing to lose DIC results");
            }
            result_server.publish(result);
            const auto summary = p2dic::summarize_result(result);
            {
                std::lock_guard lock(summary_mutex);
                latest_summary = summary;
                summary_available = true;
            }
        });
        pipeline->set_preview_callback([&](const p2dic::Frame& frame) {
            if (!config.preview_enabled) return;
            const auto period_ns = static_cast<std::uint64_t>(
                std::llround(1.0e9 / config.preview_fps));
            if (last_preview_timestamp_ns != 0 && frame.timestamp_ns > last_preview_timestamp_ns &&
                frame.timestamp_ns - last_preview_timestamp_ns < period_ns) {
                return;
            }
            auto preview = p2dic::make_preview_frame(
                frame, config.preview_maximum_width, config.preview_maximum_height);
            preview_server.publish_packet(p2dic::encode_preview_packet(preview));
            last_preview_timestamp_ns = frame.timestamp_ns;
        });
        pipeline->set_fault_callback([&state](const std::string& message) {
            state.fault();
            std::cerr << "ERROR pipeline_fault message=\"" << message << "\"\n";
        });
    };
    writer.set_fault_callback([&state](const std::string& message) {
        state.fault();
        std::cerr << "ERROR storage_fault message=\"" << message << "\"\n";
    });
    attach_pipeline_callbacks();
    state.boot_complete();

    p2dic::TcpControlServer server([&](std::string_view line) {
        std::lock_guard lock(command_mutex);
        const auto command = p2dic::parse_control_command(line);
        if (command.verb == "PING") {
            return p2dic::make_ok_response(
                "PONG", "protocol=1 result_protocol=1 result_port=" +
                            std::to_string(config.result_port) +
                            " preview_protocol=1 preview_port=" +
                            std::to_string(config.preview_enabled ? config.preview_port : 0));
        }
        if (command.verb == "STATUS") {
            const auto metrics = pipeline->metrics();
            const auto storage = writer.metrics();
            p2dic::ResultSummary summary;
            bool has_summary = false;
            {
                std::lock_guard summary_lock(summary_mutex);
                summary = latest_summary;
                has_summary = summary_available;
            }
            std::ostringstream fields;
            fields << std::fixed << std::setprecision(3)
                   << "state=" << p2dic::to_string(state.state())
                   << " result_port=" << config.result_port
                   << " preview_port=" << (config.preview_enabled ? config.preview_port : 0)
                   << " captured=" << metrics.captured
                   << " processed=" << metrics.processed
                   << " dropped=" << metrics.dropped
                   << " timeouts=" << metrics.capture_timeouts
                   << " processing_ms=" << metrics.last_processing_ms
                   << " capture_ms=" << metrics.last_capture_ms
                   << " dic_ms=" << metrics.last_dic_ms
                   << " strain_ms=" << metrics.last_strain_ms
                   << " callback_ms=" << metrics.last_callback_ms
                   << " pipeline_ms=" << metrics.last_pipeline_ms
                   << " e2e_ms=" << metrics.last_end_to_end_ms
                   << " pipeline_mean_ms=" << metrics.pipeline.mean
                   << " pipeline_p50_ms=" << metrics.pipeline.p50
                   << " pipeline_p95_ms=" << metrics.pipeline.p95
                   << " pipeline_p99_ms=" << metrics.pipeline.p99
                   << " e2e_p95_ms=" << metrics.end_to_end.p95
                   << " gpu_staging_ms=" << metrics.last_gpu_staging_ms
                   << " h2d_ms=" << metrics.last_h2d_ms
                   << " kernel_ms=" << metrics.last_kernel_ms
                   << " d2h_ms=" << metrics.last_d2h_ms
                   << " kernel_launches=" << metrics.last_kernel_launches
                   << " h2d_p95_ms=" << metrics.h2d.p95
                   << " kernel_p95_ms=" << metrics.kernel.p95
                   << " d2h_p95_ms=" << metrics.d2h.p95
                   << " timing_samples=" << metrics.pipeline.sample_count
                   << " writer_enqueued=" << storage.enqueued
                   << " writer_written=" << storage.written
                   << " writer_queue=" << storage.queue_depth
                   << " writer_queue_max=" << storage.maximum_queue_depth
                   << " writer_overruns=" << storage.overruns
                   << " writer_ms=" << storage.last_write_ms
                   << " writer_p95_ms=" << storage.write.p95
                   << " writer_faulted=" << (storage.faulted ? 1 : 0)
                   << " result_frame=" << (has_summary ? summary.frame_sequence : 0)
                   << " result_points=" << (has_summary ? summary.point_count : 0)
                   << " valid_ratio=" << (has_summary ? summary.valid_ratio : 0.0)
                   << " mean_u=" << (has_summary ? summary.mean_u : 0.0)
                   << " mean_v=" << (has_summary ? summary.mean_v : 0.0)
                   << " strain_valid_ratio=" << (has_summary ? summary.strain_valid_ratio : 0.0)
                   << " mean_exx=" << (has_summary ? summary.mean_exx : 0.0)
                   << " mean_eyy=" << (has_summary ? summary.mean_eyy : 0.0)
                   << " mean_exy=" << (has_summary ? summary.mean_exy : 0.0);
            return p2dic::make_ok_response("STATUS", fields.str());
        }
        if (command.verb == "CONFIG") {
            if (command.arguments.empty()) {
                return p2dic::make_ok_response("CONFIG", configuration_fields(config));
            }
            if (command.arguments.size() == 1 && command.arguments.front() == "SAVE") {
                if (config_path.empty()) {
                    return p2dic::make_error_response(
                        "CONFIG", "Edge was not started with --config PATH");
                }
                try {
                    p2dic::save_edge_config(config_path, config);
                    return p2dic::make_ok_response(
                        "CONFIG_SAVED", "backup=" + config_path.string() + ".bak");
                } catch (const std::exception& exception) {
                    return p2dic::make_error_response("CONFIG", exception.what());
                }
            }
            if (state.state() != p2dic::EdgeState::idle) {
                return p2dic::make_error_response(
                    "STATE", "CONFIG changes require an idle Edge");
            }
            try {
                auto candidate = config;
                for (const auto& argument : command.arguments) {
                    apply_runtime_setting(candidate, argument);
                }
                candidate.validate();
                auto replacement = make_pipeline(candidate);
                pipeline = std::move(replacement);
                config = std::move(candidate);
                last_preview_timestamp_ns = 0;
                {
                    std::lock_guard summary_lock(summary_mutex);
                    latest_summary = {};
                    summary_available = false;
                }
                attach_pipeline_callbacks();
                return p2dic::make_ok_response(
                    "CONFIGURED", configuration_fields(config));
            } catch (const std::exception& exception) {
                return p2dic::make_error_response("CONFIG", exception.what());
            }
        }
        if (command.verb == "START") {
            if (state.state() != p2dic::EdgeState::idle) {
                return p2dic::make_error_response("STATE", "Edge is not idle");
            }
            const std::string session_id = argument_value(command, "session_id");
            if (session_id.empty()) {
                return p2dic::make_error_response("ARGUMENT", "START requires session_id=VALUE");
            }
            try {
                double millimeters_per_pixel = 0.0;
                const std::string calibration = argument_value(command, "mm_per_pixel");
                if (!calibration.empty()) {
                    millimeters_per_pixel = parse_double_setting(calibration, "mm_per_pixel");
                    if (millimeters_per_pixel <= 0.0 || millimeters_per_pixel > 1000.0) {
                        throw std::invalid_argument("mm_per_pixel is outside the supported range");
                    }
                }
                writer.open(config.storage_root, p2dic::SessionInfo{
                    session_id,
                    camera_name,
                    config.runtime.width,
                    config.runtime.height,
                    config.runtime.camera_fps,
                    millimeters_per_pixel});
                state.start_measurement();
                pipeline->start();
                return p2dic::make_ok_response("STARTED", "session_id=" + session_id);
            } catch (const std::exception& exception) {
                pipeline->stop();
                if (writer.is_open()) {
                    try {
                        writer.close("faulted");
                    } catch (...) {
                    }
                }
                state.fault();
                return p2dic::make_error_response("START", exception.what());
            }
        }
        if (command.verb == "STOP") {
            if (state.state() != p2dic::EdgeState::measuring &&
                state.state() != p2dic::EdgeState::paused) {
                return p2dic::make_error_response("STATE", "No measurement is running");
            }
            pipeline->stop();
            try {
                writer.close("complete");
                state.stop_measurement();
                return p2dic::make_ok_response("STOPPED");
            } catch (const std::exception& exception) {
                state.fault();
                return p2dic::make_error_response("STORAGE", exception.what());
            }
        }
        if (command.verb == "PAUSE") {
            if (state.state() != p2dic::EdgeState::measuring) {
                return p2dic::make_error_response("STATE", "Measurement is not running");
            }
            pipeline->pause();
            state.pause_measurement();
            return p2dic::make_ok_response("PAUSED");
        }
        if (command.verb == "RESUME") {
            if (state.state() != p2dic::EdgeState::paused) {
                return p2dic::make_error_response("STATE", "Measurement is not paused");
            }
            pipeline->resume();
            state.resume_measurement();
            return p2dic::make_ok_response("RESUMED");
        }
        if (command.verb == "RESET") {
            if (state.state() != p2dic::EdgeState::faulted) {
                return p2dic::make_error_response("STATE", "Edge is not faulted");
            }
            pipeline->stop();
            if (writer.is_open()) {
                try {
                    writer.close("faulted");
                } catch (const std::exception& exception) {
                    return p2dic::make_error_response("STORAGE", exception.what());
                }
            }
            state.reset_fault();
            return p2dic::make_ok_response("RESET");
        }
        if (command.verb == "SHUTDOWN") {
            shutdown_requested = true;
            return p2dic::make_ok_response("SHUTTING_DOWN");
        }
        return p2dic::make_error_response("UNKNOWN", command.verb);
    });

    try {
        result_server.start(config.result_port, config.bind_address);
        if (config.preview_enabled) {
            preview_server.start(config.preview_port, config.bind_address);
        }
        server.start(config.control_port, config.bind_address);
    } catch (const std::exception& exception) {
        server.stop();
        result_server.stop();
        preview_server.stop();
        std::cerr << "ERROR server_start message=\"" << exception.what() << "\"\n";
        return 1;
    }
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);
    std::cout << "INFO dic_edge_ready port=" << config.control_port
              << " result_port=" << config.result_port
              << " preview_port=" << (config.preview_enabled ? config.preview_port : 0)
              << " camera=" << p2dic::to_string(config.camera_backend)
              << " dic=" << p2dic::to_string(config.dic_backend)
              << " data_root=" << config.storage_root.string() << '\n';

    using namespace std::chrono_literals;
    std::thread self_test_thread;
    if (self_test) {
        self_test_thread = std::thread([&] {
            try {
                const auto unique_suffix = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                const std::string self_test_session =
                    "self-test-" + std::to_string(unique_suffix);
                const auto ping = p2dic::send_control_command("127.0.0.1", config.control_port, "PING");
                const auto initial = p2dic::send_control_command("127.0.0.1", config.control_port, "STATUS");
                const auto configuration = p2dic::send_control_command(
                    "127.0.0.1", config.control_port, "CONFIG");
                const auto configured = p2dic::send_control_command(
                    "127.0.0.1", config.control_port,
                    "CONFIG camera.exposure_us=1500 camera.gain_db=1.5");
                const auto started = p2dic::send_control_command(
                    "127.0.0.1", config.control_port, "START session_id=" + self_test_session);
                const auto streamed_result = p2dic::receive_latest_result(
                    "127.0.0.1", config.result_port, 3s);
                const auto preview_packet = p2dic::receive_latest_packet(
                    "127.0.0.1", config.preview_port, 64U * 1024U * 1024U, 3s);
                const auto streamed_preview = p2dic::decode_preview_packet(preview_packet);
                std::this_thread::sleep_for(1500ms);
                const auto measuring = p2dic::send_control_command("127.0.0.1", config.control_port, "STATUS");
                const auto paused = p2dic::send_control_command(
                    "127.0.0.1", config.control_port, "PAUSE");
                const auto paused_status = p2dic::send_control_command(
                    "127.0.0.1", config.control_port, "STATUS");
                const auto resumed = p2dic::send_control_command(
                    "127.0.0.1", config.control_port, "RESUME");
                const auto stopped = p2dic::send_control_command("127.0.0.1", config.control_port, "STOP");
                const auto stored = p2dic::scan_session_binary(
                    config.storage_root / self_test_session / "results.p2dic");
                const auto shutting_down =
                    p2dic::send_control_command("127.0.0.1", config.control_port, "SHUTDOWN");
                if (!ping.starts_with("OK PONG") ||
                    initial.find("state=idle") == std::string::npos ||
                    configuration.find("image.width=512") == std::string::npos ||
                    configured.find("camera.exposure_us=1500") == std::string::npos ||
                    !started.starts_with("OK STARTED") ||
                    streamed_result.points.empty() ||
                    streamed_preview.pixels.empty() ||
                    measuring.find("state=measuring") == std::string::npos ||
                    measuring.find("result_points=0") != std::string::npos ||
                    measuring.find("valid_ratio=") == std::string::npos ||
                    !paused.starts_with("OK PAUSED") ||
                    paused_status.find("state=paused") == std::string::npos ||
                    !resumed.starts_with("OK RESUMED") ||
                    !stopped.starts_with("OK STOPPED") ||
                    stored.results.empty() || stored.truncated_or_corrupt ||
                    !shutting_down.starts_with("OK SHUTTING_DOWN")) {
                    throw std::runtime_error("Unexpected DIC Edge self-test response");
                }
            } catch (const std::exception& exception) {
                self_test_failed = true;
                shutdown_requested = true;
                std::cerr << "ERROR self_test message=\"" << exception.what() << "\"\n";
            }
        });
    }
    while (!shutdown_requested && signal_requested == 0) {
        std::this_thread::sleep_for(100ms);
    }
    server.stop();
    {
        std::lock_guard lock(command_mutex);
        state.shutdown();
        pipeline->stop();
        if (writer.is_open()) {
            writer.close("interrupted");
        }
    }
    result_server.stop();
    preview_server.stop();
    if (self_test_thread.joinable()) {
        self_test_thread.join();
    }
    std::cout << "INFO dic_edge_stopped\n";
    return self_test_failed ? 2 : 0;
}
