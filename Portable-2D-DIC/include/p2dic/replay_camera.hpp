#pragma once

#include "p2dic/camera.hpp"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace p2dic {

struct ReplayCameraConfig {
    std::filesystem::path directory;
    double frames_per_second{20.0};
    bool loop{true};
};

class ReplayCamera final : public ICamera {
public:
    explicit ReplayCamera(ReplayCameraConfig config);

    void open() override;
    void start() override;
    bool grab(Frame& destination, std::chrono::milliseconds timeout) override;
    void stop() override;
    void close() override;
    [[nodiscard]] std::string_view name() const noexcept override;

private:
    ReplayCameraConfig config_;
    std::vector<std::filesystem::path> files_;
    std::size_t next_index_{0};
    std::uint64_t sequence_{0};
    bool opened_{false};
    bool streaming_{false};
};

}  // namespace p2dic
