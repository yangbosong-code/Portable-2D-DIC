#include "p2dic/edge_config.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

int main() {
    const auto root = std::filesystem::temp_directory_path() / "p2dic_config_test";
    std::filesystem::create_directories(root);
    const auto valid_path = root / "valid.conf";
    {
        std::ofstream stream(valid_path);
        stream << "camera.backend=synthetic\n"
               << "synthetic.dx_per_frame=0.02\nsynthetic.dy_per_frame=-0.01\n"
               << "synthetic.motion=sinusoidal\nsynthetic.amplitude_x=2.5\n"
               << "synthetic.amplitude_y=1.25\nsynthetic.frequency_hz=0.2\n"
               << "image.width=640\nimage.height=480\nimage.fps=20\n"
               << "dic.backend=cpu\ndic.subset_radius=12\ndic.grid_step=32\n"
               << "dic.recovery_trigger_valid_ratio=0.92\ndic.max_recovery_passes=2\n"
               << "runtime.frame_pool_size=7\nruntime.capture_queue_size=3\n"
               << "storage.root=data\nnetwork.control_port=19000\nnetwork.result_port=19001\n"
               << "network.preview_port=19002\npreview.enabled=true\n"
               << "preview.maximum_width=800\npreview.maximum_height=600\npreview.fps=8\n";
    }
    try {
        auto config = p2dic::load_edge_config(valid_path);
        if (config.runtime.width != 640 || config.runtime.height != 480 ||
            config.runtime.subset_radius != 12 || config.control_port != 19000 ||
            config.result_port != 19001 ||
            config.preview_port != 19002 || config.preview_maximum_width != 800 ||
            config.preview_maximum_height != 600 || config.preview_fps != 8.0 ||
            config.synthetic_dx_per_frame != 0.02 ||
            config.synthetic_dy_per_frame != -0.01 ||
            config.synthetic_motion_mode != p2dic::SyntheticMotionMode::sinusoidal ||
            config.synthetic_displacement_x_amplitude != 2.5 ||
            config.synthetic_displacement_y_amplitude != 1.25 ||
            config.synthetic_motion_frequency_hz != 0.2 ||
            config.recovery_trigger_valid_ratio != 0.92 ||
            config.max_recovery_passes != 2) {
            return 1;
        }
        config.galaxy.exposure_us = 3456.0;
        config.galaxy.gain_db = 2.5;
        config.runtime.grid_step = 24;
        config.inverse_compositional = true;
        p2dic::save_edge_config(valid_path, config);
        const auto reloaded = p2dic::load_edge_config(valid_path);
        if (reloaded.galaxy.exposure_us != 3456.0 ||
            reloaded.galaxy.gain_db != 2.5 || reloaded.runtime.grid_step != 24 ||
            reloaded.synthetic_dx_per_frame != 0.02 ||
            reloaded.synthetic_dy_per_frame != -0.01 ||
            reloaded.synthetic_motion_mode != p2dic::SyntheticMotionMode::sinusoidal ||
            reloaded.synthetic_displacement_x_amplitude != 2.5 ||
            reloaded.synthetic_displacement_y_amplitude != 1.25 ||
            reloaded.synthetic_motion_frequency_hz != 0.2 ||
            !reloaded.inverse_compositional ||
            !std::filesystem::exists(valid_path.string() + ".bak")) {
            std::cerr << "Saved configuration did not round trip\n";
            return 2;
        }
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 3;
    }
    const auto invalid_path = root / "invalid.conf";
    {
        std::ofstream stream(invalid_path);
        stream << "image.width=640\nunknown.option=true\n";
    }
    bool rejected = false;
    try {
        static_cast<void>(p2dic::load_edge_config(invalid_path));
    } catch (const std::exception&) {
        rejected = true;
    }
    std::error_code error;
    std::filesystem::remove_all(root, error);
    return rejected ? 0 : 4;
}
