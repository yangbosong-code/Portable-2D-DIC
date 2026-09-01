#pragma once

#include "p2dic/galaxy_camera.hpp"
#include "p2dic/runtime_config.hpp"
#include "p2dic/synthetic_camera.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace p2dic {

enum class CameraBackend { synthetic, replay, galaxy };
enum class DicBackend { cpu, cuda };

struct EdgeConfig {
    RuntimeConfig runtime;
    CameraBackend camera_backend{CameraBackend::synthetic};
    DicBackend dic_backend{DicBackend::cpu};
    GalaxyCameraConfig galaxy;
    std::filesystem::path replay_directory;
    bool replay_loop{true};
    std::uint32_t synthetic_seed{7};
    double synthetic_dx_per_frame{1.0};
    double synthetic_dy_per_frame{0.0};
    SyntheticMotionMode synthetic_motion_mode{SyntheticMotionMode::linear};
    double synthetic_displacement_x_amplitude{2.0};
    double synthetic_displacement_y_amplitude{1.0};
    double synthetic_motion_frequency_hz{0.1};
    int max_iterations{25};
    double convergence_tolerance{1.0e-4};
    double quality_threshold{0.80};
    double maximum_iteration_step{1.0};
    double recovery_trigger_valid_ratio{0.95};
    int max_recovery_passes{1};
    bool inverse_compositional{false};
    std::string bind_address{"0.0.0.0"};
    std::uint16_t control_port{17840};
    std::uint16_t result_port{17841};
    std::uint16_t preview_port{17842};
    bool preview_enabled{true};
    std::uint32_t preview_maximum_width{960};
    std::uint32_t preview_maximum_height{720};
    double preview_fps{10.0};
    std::filesystem::path storage_root{"sessions"};
    bool storage_csv_enabled{false};
    std::size_t storage_queue_capacity{64};
    std::size_t storage_flush_every_frames{30};

    void validate() const;
};

EdgeConfig load_edge_config(const std::filesystem::path& path);
void save_edge_config(const std::filesystem::path& path, const EdgeConfig& config);
[[nodiscard]] std::string_view to_string(CameraBackend backend) noexcept;
[[nodiscard]] std::string_view to_string(DicBackend backend) noexcept;
[[nodiscard]] std::string_view to_string(SyntheticMotionMode mode) noexcept;

}  // namespace p2dic
