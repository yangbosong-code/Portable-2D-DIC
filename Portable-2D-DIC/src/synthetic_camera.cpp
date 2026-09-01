#include "p2dic/synthetic_camera.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <thread>

namespace p2dic {
namespace {

std::uint64_t now_ns() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

}  // namespace

SyntheticCamera::SyntheticCamera(SyntheticCameraConfig config)
    : config_(config),
      reference_(config.width, config.height),
      random_(config.seed) {
    if (config_.width < 64 || config_.height < 64) {
        throw std::invalid_argument("Synthetic image must be at least 64 x 64");
    }
    if (config_.frames_per_second <= 0.0) {
        throw std::invalid_argument("Frame rate must be positive");
    }
    if (!std::isfinite(config_.displacement_x_per_frame) ||
        !std::isfinite(config_.displacement_y_per_frame) ||
        !std::isfinite(config_.displacement_x_amplitude) ||
        !std::isfinite(config_.displacement_y_amplitude) ||
        !std::isfinite(config_.motion_frequency_hz)) {
        throw std::invalid_argument("Synthetic motion parameters must be finite");
    }
    if (config_.displacement_x_amplitude < 0.0 ||
        config_.displacement_y_amplitude < 0.0 ||
        config_.motion_frequency_hz <= 0.0 ||
        config_.motion_frequency_hz > config_.frames_per_second * 0.5) {
        throw std::invalid_argument("Synthetic sinusoidal motion parameters are outside the supported range");
    }
}

void SyntheticCamera::open() {
    if (!opened_) {
        generate_reference();
        opened_ = true;
    }
}

void SyntheticCamera::start() {
    if (!opened_) {
        throw std::logic_error("Camera must be opened before streaming");
    }
    sequence_ = 0;
    streaming_ = true;
}

bool SyntheticCamera::grab(Frame& destination, std::chrono::milliseconds timeout) {
    if (!streaming_) {
        throw std::logic_error("Camera is not streaming");
    }

    const auto frame_period = std::chrono::duration<double>(1.0 / config_.frames_per_second);
    if (frame_period > timeout) {
        return false;
    }

    std::this_thread::sleep_for(frame_period);
    double dx = static_cast<double>(sequence_) * config_.displacement_x_per_frame;
    double dy = static_cast<double>(sequence_) * config_.displacement_y_per_frame;
    if (config_.motion_mode == SyntheticMotionMode::sinusoidal) {
        constexpr double two_pi = 6.28318530717958647692;
        const double time_seconds = static_cast<double>(sequence_) / config_.frames_per_second;
        const double phase = two_pi * config_.motion_frequency_hz * time_seconds;
        dx = config_.displacement_x_amplitude * std::sin(phase);
        // Use a second harmonic for Y. Both axes start at zero, so the first
        // generated image is an exact and repeatable DIC reference frame.
        dy = config_.displacement_y_amplitude * std::sin(2.0 * phase);
    }
    render_shifted(destination, dx, dy);
    destination.sequence = sequence_++;
    destination.timestamp_ns = now_ns();
    return true;
}

void SyntheticCamera::stop() {
    streaming_ = false;
}

void SyntheticCamera::close() {
    streaming_ = false;
    opened_ = false;
}

std::string_view SyntheticCamera::name() const noexcept {
    return "SyntheticCamera";
}

void SyntheticCamera::generate_reference() {
    std::uniform_int_distribution<int> center_x(0, static_cast<int>(config_.width) - 1);
    std::uniform_int_distribution<int> center_y(0, static_cast<int>(config_.height) - 1);
    std::uniform_int_distribution<int> radius_distribution(2, 6);
    std::uniform_int_distribution<int> gray_distribution(15, 70);

    std::fill(reference_.pixels.begin(), reference_.pixels.end(), static_cast<std::uint8_t>(225));
    const std::size_t speckle_count =
        static_cast<std::size_t>(config_.width) * config_.height / 45;

    for (std::size_t i = 0; i < speckle_count; ++i) {
        const int cx = center_x(random_);
        const int cy = center_y(random_);
        const int radius = radius_distribution(random_);
        const auto gray = static_cast<std::uint8_t>(gray_distribution(random_));

        for (int y = std::max(0, cy - radius);
             y <= std::min(static_cast<int>(config_.height) - 1, cy + radius);
             ++y) {
            for (int x = std::max(0, cx - radius);
                 x <= std::min(static_cast<int>(config_.width) - 1, cx + radius);
                 ++x) {
                const int xx = x - cx;
                const int yy = y - cy;
                if (xx * xx + yy * yy <= radius * radius) {
                    reference_.pixels[static_cast<std::size_t>(y) * reference_.stride + x] = gray;
                }
            }
        }
    }
    // A physical lens and sensor never produce perfectly sharp binary speckle
    // edges. Mild optical blur also makes sub-pixel interpolation differentiable,
    // which is required by the Gauss-Newton DIC refinement used in the demo.
    apply_optical_blur();
}

void SyntheticCamera::apply_optical_blur() {
    Frame current = reference_;
    for (int pass = 0; pass < 3; ++pass) {
        Frame next = current;
        for (int y = 1; y + 1 < static_cast<int>(current.height); ++y) {
            for (int x = 1; x + 1 < static_cast<int>(current.width); ++x) {
                int sum = 0;
                for (int ky = -1; ky <= 1; ++ky) {
                    for (int kx = -1; kx <= 1; ++kx) {
                        sum += current.pixels[
                            static_cast<std::size_t>(y + ky) * current.stride + x + kx];
                    }
                }
                next.pixels[static_cast<std::size_t>(y) * next.stride + x] =
                    static_cast<std::uint8_t>((sum + 4) / 9);
            }
        }
        current = std::move(next);
    }
    reference_ = std::move(current);
}

void SyntheticCamera::render_shifted(Frame& destination, double dx, double dy) const {
    if (destination.width != config_.width || destination.height != config_.height) {
        destination = Frame(config_.width, config_.height);
    }
    std::fill(destination.pixels.begin(), destination.pixels.end(), static_cast<std::uint8_t>(225));

    const double maximum_x = static_cast<double>(config_.width - 1);
    const double maximum_y = static_cast<double>(config_.height - 1);
    for (int y = 0; y < static_cast<int>(config_.height); ++y) {
        const double source_y = static_cast<double>(y) - dy;
        if (source_y < 0.0 || source_y > maximum_y) {
            continue;
        }
        for (int x = 0; x < static_cast<int>(config_.width); ++x) {
            const double source_x = static_cast<double>(x) - dx;
            if (source_x < 0.0 || source_x > maximum_x) {
                continue;
            }

            const int x0 = static_cast<int>(std::floor(source_x));
            const int y0 = static_cast<int>(std::floor(source_y));
            const int x1 = std::min(x0 + 1, static_cast<int>(config_.width) - 1);
            const int y1 = std::min(y0 + 1, static_cast<int>(config_.height) - 1);
            const double fx = source_x - static_cast<double>(x0);
            const double fy = source_y - static_cast<double>(y0);
            const auto sample = [this](int sample_x, int sample_y) {
                return static_cast<double>(reference_.pixels[
                    static_cast<std::size_t>(sample_y) * reference_.stride + sample_x]);
            };
            const double top = sample(x0, y0) * (1.0 - fx) + sample(x1, y0) * fx;
            const double bottom = sample(x0, y1) * (1.0 - fx) + sample(x1, y1) * fx;
            const double value = top * (1.0 - fy) + bottom * fy;
            destination.pixels[static_cast<std::size_t>(y) * destination.stride + x] =
                static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L));
        }
    }
}

}  // namespace p2dic
