#pragma once

#include "p2dic/camera.hpp"

#include <cstdint>
#include <string>

namespace p2dic {

struct GalaxyCameraConfig {
    std::string serial_number;
    std::uint32_t device_index{1};
    std::uint32_t width{4504};
    std::uint32_t height{4096};
    std::uint32_t offset_x{0};
    std::uint32_t offset_y{0};
    double exposure_us{2000.0};
    double gain_db{0.0};
    bool external_trigger{false};
};

#if defined(P2DIC_WITH_GALAXY)

class GalaxyCamera final : public ICamera {
public:
    explicit GalaxyCamera(GalaxyCameraConfig config = {});
    ~GalaxyCamera() override;

    void open() override;
    void start() override;
    bool grab(Frame& destination, std::chrono::milliseconds timeout) override;
    void stop() override;
    void close() override;
    [[nodiscard]] std::string_view name() const noexcept override;

private:
    GalaxyCameraConfig config_;
    void* device_{nullptr};
    bool library_open_{false};
    bool streaming_{false};
};

#endif

}  // namespace p2dic
