#include "p2dic/edge_config.hpp"

#include <charconv>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <stdexcept>
#include <string_view>

namespace p2dic {
namespace {

std::string trim(std::string_view value) {
    std::size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) ++begin;
    std::size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return std::string(value.substr(begin, end - begin));
}

template <typename Integer>
Integer parse_integer(const std::string& value, std::size_t line) {
    Integer output{};
    const auto result = std::from_chars(value.data(), value.data() + value.size(), output);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
        throw std::runtime_error("Invalid integer at configuration line " + std::to_string(line));
    }
    return output;
}

double parse_double(const std::string& value, std::size_t line) {
    std::size_t parsed = 0;
    double output = 0.0;
    try {
        output = std::stod(value, &parsed);
    } catch (...) {
        throw std::runtime_error("Invalid number at configuration line " + std::to_string(line));
    }
    if (parsed != value.size()) {
        throw std::runtime_error("Invalid number at configuration line " + std::to_string(line));
    }
    return output;
}

bool parse_bool(const std::string& value, std::size_t line) {
    if (value == "true") return true;
    if (value == "false") return false;
    throw std::runtime_error("Expected true or false at configuration line " + std::to_string(line));
}

}  // namespace

void EdgeConfig::validate() const {
    runtime.validate();
    if (!std::isfinite(runtime.camera_fps) || runtime.camera_fps > 1000.0 ||
        !std::isfinite(galaxy.exposure_us) || galaxy.exposure_us < 11.0 ||
        galaxy.exposure_us > 1.0e6 || !std::isfinite(galaxy.gain_db) ||
        galaxy.gain_db < 0.0 || galaxy.gain_db > 16.0 ||
        !std::isfinite(synthetic_dx_per_frame) ||
        !std::isfinite(synthetic_dy_per_frame) ||
        !std::isfinite(synthetic_displacement_x_amplitude) ||
        !std::isfinite(synthetic_displacement_y_amplitude) ||
        !std::isfinite(synthetic_motion_frequency_hz)) {
        throw std::invalid_argument("Camera FPS, exposure, or gain is outside the supported range");
    }
    if (synthetic_displacement_x_amplitude < 0.0 ||
        synthetic_displacement_y_amplitude < 0.0 ||
        synthetic_motion_frequency_hz <= 0.0 ||
        synthetic_motion_frequency_hz > runtime.camera_fps * 0.5) {
        throw std::invalid_argument("Synthetic sinusoidal motion configuration is invalid");
    }
    if (control_port == 0 || result_port == 0 || preview_port == 0 ||
        control_port == result_port || control_port == preview_port ||
        result_port == preview_port || preview_maximum_width == 0 ||
        preview_maximum_height == 0 || preview_maximum_width > 8192 ||
        preview_maximum_height > 8192 || preview_fps <= 0.0 ||
        bind_address.empty() || storage_root.empty() || storage_queue_capacity < 4 ||
        storage_queue_capacity > 4096 || storage_flush_every_frames == 0 ||
        storage_flush_every_frames > 4096) {
        throw std::invalid_argument("Network or storage configuration is incomplete");
    }
    if (max_iterations < 1 || convergence_tolerance <= 0.0 ||
        quality_threshold < -1.0 || quality_threshold > 1.0 ||
        maximum_iteration_step <= 0.0 || recovery_trigger_valid_ratio < 0.0 ||
        recovery_trigger_valid_ratio > 1.0 || max_recovery_passes < 0 ||
        max_recovery_passes > 4) {
        throw std::invalid_argument("DIC solver configuration is invalid");
    }
    if (camera_backend == CameraBackend::replay && replay_directory.empty()) {
        throw std::invalid_argument("Replay camera requires camera.replay_directory");
    }
    if (camera_backend == CameraBackend::galaxy &&
        (galaxy.width != runtime.width || galaxy.height != runtime.height)) {
        throw std::invalid_argument("Galaxy dimensions must match image.width and image.height");
    }
    if (camera_backend == CameraBackend::galaxy &&
        (static_cast<std::uint64_t>(galaxy.offset_x) + galaxy.width > 4504ULL ||
         static_cast<std::uint64_t>(galaxy.offset_y) + galaxy.height > 4096ULL)) {
        throw std::invalid_argument("Galaxy ROI exceeds the ME2P-1840-21U3M sensor bounds");
    }
}

EdgeConfig load_edge_config(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream) {
        throw std::runtime_error("Unable to open DIC Edge configuration: " + path.string());
    }
    EdgeConfig config;
    std::set<std::string> seen;
    std::string line_text;
    std::size_t line_number = 0;
    while (std::getline(stream, line_text)) {
        ++line_number;
        const auto comment = line_text.find('#');
        if (comment != std::string::npos) line_text.resize(comment);
        line_text = trim(line_text);
        if (line_text.empty()) continue;
        const auto equal = line_text.find('=');
        if (equal == std::string::npos) {
            throw std::runtime_error("Expected key=value at configuration line " +
                                     std::to_string(line_number));
        }
        const std::string key = trim(std::string_view(line_text).substr(0, equal));
        const std::string value = trim(std::string_view(line_text).substr(equal + 1));
        if (key.empty() || value.empty()) {
            throw std::runtime_error("Empty key or value at configuration line " +
                                     std::to_string(line_number));
        }
        if (!seen.insert(key).second) {
            throw std::runtime_error("Duplicate configuration key: " + key);
        }

        if (key == "camera.backend") {
            if (value == "synthetic") config.camera_backend = CameraBackend::synthetic;
            else if (value == "replay") config.camera_backend = CameraBackend::replay;
            else if (value == "galaxy") config.camera_backend = CameraBackend::galaxy;
            else throw std::runtime_error("Unknown camera backend at line " + std::to_string(line_number));
        } else if (key == "camera.serial_number") {
            config.galaxy.serial_number = value == "AUTO" ? std::string{} : value;
        }
        else if (key == "camera.device_index") config.galaxy.device_index = parse_integer<std::uint32_t>(value, line_number);
        else if (key == "camera.offset_x") config.galaxy.offset_x = parse_integer<std::uint32_t>(value, line_number);
        else if (key == "camera.offset_y") config.galaxy.offset_y = parse_integer<std::uint32_t>(value, line_number);
        else if (key == "camera.exposure_us") config.galaxy.exposure_us = parse_double(value, line_number);
        else if (key == "camera.gain_db") config.galaxy.gain_db = parse_double(value, line_number);
        else if (key == "camera.external_trigger") config.galaxy.external_trigger = parse_bool(value, line_number);
        else if (key == "camera.replay_directory") config.replay_directory = value;
        else if (key == "camera.replay_loop") config.replay_loop = parse_bool(value, line_number);
        else if (key == "synthetic.seed") config.synthetic_seed = parse_integer<std::uint32_t>(value, line_number);
        else if (key == "synthetic.dx_per_frame") config.synthetic_dx_per_frame = parse_double(value, line_number);
        else if (key == "synthetic.dy_per_frame") config.synthetic_dy_per_frame = parse_double(value, line_number);
        else if (key == "synthetic.motion") {
            if (value == "linear") config.synthetic_motion_mode = SyntheticMotionMode::linear;
            else if (value == "sinusoidal") config.synthetic_motion_mode = SyntheticMotionMode::sinusoidal;
            else throw std::runtime_error("Unknown synthetic motion at line " + std::to_string(line_number));
        }
        else if (key == "synthetic.amplitude_x") config.synthetic_displacement_x_amplitude = parse_double(value, line_number);
        else if (key == "synthetic.amplitude_y") config.synthetic_displacement_y_amplitude = parse_double(value, line_number);
        else if (key == "synthetic.frequency_hz") config.synthetic_motion_frequency_hz = parse_double(value, line_number);
        else if (key == "image.width") config.runtime.width = config.galaxy.width = parse_integer<std::uint32_t>(value, line_number);
        else if (key == "image.height") config.runtime.height = config.galaxy.height = parse_integer<std::uint32_t>(value, line_number);
        else if (key == "image.fps") config.runtime.camera_fps = parse_double(value, line_number);
        else if (key == "dic.backend") {
            if (value == "cpu") config.dic_backend = DicBackend::cpu;
            else if (value == "cuda") config.dic_backend = DicBackend::cuda;
            else throw std::runtime_error("Unknown DIC backend at line " + std::to_string(line_number));
        } else if (key == "dic.subset_radius") config.runtime.subset_radius = parse_integer<int>(value, line_number);
        else if (key == "dic.grid_step") config.runtime.grid_step = parse_integer<int>(value, line_number);
        else if (key == "dic.search_radius") config.runtime.search_radius = parse_integer<int>(value, line_number);
        else if (key == "dic.max_iterations") config.max_iterations = parse_integer<int>(value, line_number);
        else if (key == "dic.convergence_tolerance") config.convergence_tolerance = parse_double(value, line_number);
        else if (key == "dic.quality_threshold") config.quality_threshold = parse_double(value, line_number);
        else if (key == "dic.maximum_iteration_step") config.maximum_iteration_step = parse_double(value, line_number);
        else if (key == "dic.recovery_trigger_valid_ratio") config.recovery_trigger_valid_ratio = parse_double(value, line_number);
        else if (key == "dic.max_recovery_passes") config.max_recovery_passes = parse_integer<int>(value, line_number);
        else if (key == "dic.inverse_compositional") config.inverse_compositional = parse_bool(value, line_number);
        else if (key == "runtime.frame_pool_size") config.runtime.frame_pool_size = parse_integer<std::size_t>(value, line_number);
        else if (key == "runtime.capture_queue_size") config.runtime.capture_queue_size = parse_integer<std::size_t>(value, line_number);
        else if (key == "network.bind_address") config.bind_address = value;
        else if (key == "network.control_port") config.control_port = parse_integer<std::uint16_t>(value, line_number);
        else if (key == "network.result_port") config.result_port = parse_integer<std::uint16_t>(value, line_number);
        else if (key == "network.preview_port") config.preview_port = parse_integer<std::uint16_t>(value, line_number);
        else if (key == "preview.enabled") config.preview_enabled = parse_bool(value, line_number);
        else if (key == "preview.maximum_width") config.preview_maximum_width = parse_integer<std::uint32_t>(value, line_number);
        else if (key == "preview.maximum_height") config.preview_maximum_height = parse_integer<std::uint32_t>(value, line_number);
        else if (key == "preview.fps") config.preview_fps = parse_double(value, line_number);
        else if (key == "storage.root") config.storage_root = value;
        else if (key == "storage.csv_enabled") config.storage_csv_enabled = parse_bool(value, line_number);
        else if (key == "storage.queue_capacity") config.storage_queue_capacity = parse_integer<std::size_t>(value, line_number);
        else if (key == "storage.flush_every_frames") config.storage_flush_every_frames = parse_integer<std::size_t>(value, line_number);
        else throw std::runtime_error("Unknown configuration key: " + key);
    }
    config.validate();
    return config;
}

void save_edge_config(const std::filesystem::path& path, const EdgeConfig& config) {
    config.validate();
    if (path.empty()) {
        throw std::invalid_argument("Configuration path is empty");
    }
    const auto temporary = std::filesystem::path(path.string() + ".partial");
    const auto backup = std::filesystem::path(path.string() + ".bak");
    {
        std::ofstream stream(temporary, std::ios::out | std::ios::trunc);
        if (!stream) throw std::runtime_error("Unable to create temporary configuration");
        stream << std::boolalpha << std::setprecision(12)
               << "camera.backend=" << to_string(config.camera_backend) << '\n'
               << "camera.serial_number=" << (config.galaxy.serial_number.empty() ? "AUTO" : config.galaxy.serial_number) << '\n'
               << "camera.device_index=" << config.galaxy.device_index << '\n'
               << "camera.offset_x=" << config.galaxy.offset_x << '\n'
               << "camera.offset_y=" << config.galaxy.offset_y << '\n'
               << "camera.exposure_us=" << config.galaxy.exposure_us << '\n'
               << "camera.gain_db=" << config.galaxy.gain_db << '\n'
               << "camera.external_trigger=" << config.galaxy.external_trigger << '\n'
               << "camera.replay_directory=" << (config.replay_directory.empty() ? "." : config.replay_directory.string()) << '\n'
               << "camera.replay_loop=" << config.replay_loop << '\n'
               << "synthetic.seed=" << config.synthetic_seed << '\n'
               << "synthetic.dx_per_frame=" << config.synthetic_dx_per_frame << '\n'
               << "synthetic.dy_per_frame=" << config.synthetic_dy_per_frame << '\n'
               << "synthetic.motion=" << to_string(config.synthetic_motion_mode) << '\n'
               << "synthetic.amplitude_x=" << config.synthetic_displacement_x_amplitude << '\n'
               << "synthetic.amplitude_y=" << config.synthetic_displacement_y_amplitude << '\n'
               << "synthetic.frequency_hz=" << config.synthetic_motion_frequency_hz << '\n'
               << "image.width=" << config.runtime.width << '\n'
               << "image.height=" << config.runtime.height << '\n'
               << "image.fps=" << config.runtime.camera_fps << '\n'
               << "dic.backend=" << to_string(config.dic_backend) << '\n'
               << "dic.subset_radius=" << config.runtime.subset_radius << '\n'
               << "dic.grid_step=" << config.runtime.grid_step << '\n'
               << "dic.search_radius=" << config.runtime.search_radius << '\n'
               << "dic.max_iterations=" << config.max_iterations << '\n'
               << "dic.convergence_tolerance=" << config.convergence_tolerance << '\n'
               << "dic.quality_threshold=" << config.quality_threshold << '\n'
               << "dic.maximum_iteration_step=" << config.maximum_iteration_step << '\n'
               << "dic.recovery_trigger_valid_ratio=" << config.recovery_trigger_valid_ratio << '\n'
               << "dic.max_recovery_passes=" << config.max_recovery_passes << '\n'
               << "dic.inverse_compositional=" << config.inverse_compositional << '\n'
               << "runtime.frame_pool_size=" << config.runtime.frame_pool_size << '\n'
               << "runtime.capture_queue_size=" << config.runtime.capture_queue_size << '\n'
               << "network.bind_address=" << config.bind_address << '\n'
               << "network.control_port=" << config.control_port << '\n'
               << "network.result_port=" << config.result_port << '\n'
               << "network.preview_port=" << config.preview_port << '\n'
               << "preview.enabled=" << config.preview_enabled << '\n'
               << "preview.maximum_width=" << config.preview_maximum_width << '\n'
               << "preview.maximum_height=" << config.preview_maximum_height << '\n'
               << "preview.fps=" << config.preview_fps << '\n'
               << "storage.root=" << config.storage_root.string() << '\n';
        stream << "storage.csv_enabled=" << config.storage_csv_enabled << '\n'
               << "storage.queue_capacity=" << config.storage_queue_capacity << '\n'
               << "storage.flush_every_frames=" << config.storage_flush_every_frames << '\n';
        stream.flush();
        if (!stream) throw std::runtime_error("Writing temporary configuration failed");
    }
    std::error_code error;
    if (std::filesystem::exists(path, error)) {
        std::filesystem::copy_file(
            path, backup, std::filesystem::copy_options::overwrite_existing, error);
        if (error) throw std::runtime_error("Unable to back up configuration: " + error.message());
        std::filesystem::remove(path, error);
        if (error) throw std::runtime_error("Unable to replace configuration: " + error.message());
    }
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::error_code restore_error;
        if (std::filesystem::exists(backup, restore_error)) {
            std::filesystem::copy_file(
                backup, path, std::filesystem::copy_options::overwrite_existing, restore_error);
        }
        throw std::runtime_error("Unable to commit configuration: " + error.message());
    }
}

std::string_view to_string(CameraBackend backend) noexcept {
    switch (backend) {
        case CameraBackend::synthetic: return "synthetic";
        case CameraBackend::replay: return "replay";
        case CameraBackend::galaxy: return "galaxy";
    }
    return "unknown";
}

std::string_view to_string(DicBackend backend) noexcept {
    switch (backend) {
        case DicBackend::cpu: return "cpu";
        case DicBackend::cuda: return "cuda";
    }
    return "unknown";
}

std::string_view to_string(SyntheticMotionMode mode) noexcept {
    switch (mode) {
        case SyntheticMotionMode::linear: return "linear";
        case SyntheticMotionMode::sinusoidal: return "sinusoidal";
    }
    return "unknown";
}

}  // namespace p2dic
