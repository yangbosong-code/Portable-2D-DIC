#include "p2dic/replay_camera.hpp"

#include "p2dic/pgm_io.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace p2dic {
namespace {

std::uint64_t steady_now_ns() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

}  // namespace

ReplayCamera::ReplayCamera(ReplayCameraConfig config) : config_(std::move(config)) {
    if (config_.frames_per_second <= 0.0) {
        throw std::invalid_argument("Replay FPS must be positive");
    }
}

void ReplayCamera::open() {
    files_.clear();
    if (!std::filesystem::is_directory(config_.directory)) {
        throw std::runtime_error("Replay directory does not exist: " + config_.directory.string());
    }
    for (const auto& entry : std::filesystem::directory_iterator(config_.directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".pgm") {
            files_.push_back(entry.path());
        }
    }
    std::sort(files_.begin(), files_.end());
    if (files_.empty()) {
        throw std::runtime_error("Replay directory contains no .pgm frames");
    }
    opened_ = true;
}

void ReplayCamera::start() {
    if (!opened_) {
        throw std::logic_error("Replay camera must be opened first");
    }
    next_index_ = 0;
    sequence_ = 0;
    streaming_ = true;
}

bool ReplayCamera::grab(Frame& destination, std::chrono::milliseconds timeout) {
    if (!streaming_) {
        throw std::logic_error("Replay camera is not streaming");
    }
    const auto frame_period = std::chrono::duration<double>(1.0 / config_.frames_per_second);
    if (frame_period > timeout) {
        return false;
    }
    if (next_index_ >= files_.size()) {
        if (!config_.loop) {
            return false;
        }
        next_index_ = 0;
    }
    std::this_thread::sleep_for(frame_period);
    destination = load_pgm(files_[next_index_++]);
    destination.sequence = sequence_++;
    destination.timestamp_ns = steady_now_ns();
    return true;
}

void ReplayCamera::stop() {
    streaming_ = false;
}

void ReplayCamera::close() {
    streaming_ = false;
    opened_ = false;
    files_.clear();
}

std::string_view ReplayCamera::name() const noexcept {
    return "ReplayCamera";
}

}  // namespace p2dic
