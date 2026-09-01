#pragma once

#include "p2dic/frame.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace p2dic {

struct DicPoint {
    int x{};
    int y{};
    float u{};
    float v{};
    float quality{};
    bool valid{false};
    float exx{};
    float eyy{};
    float exy{};
    bool strain_valid{false};
};

struct DicResult {
    std::uint64_t frame_sequence{};
    std::uint64_t frame_timestamp_ns{};
    double processing_ms{};
    std::vector<DicPoint> points;
};

struct EngineStageTiming {
    double host_staging_ms{};
    double h2d_ms{};
    double kernel_ms{};
    double d2h_ms{};
    int kernel_launches{};
};

class IDicEngine {
public:
    virtual ~IDicEngine() = default;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    virtual DicResult process(const Frame& reference, const Frame& deformed) = 0;
    [[nodiscard]] virtual EngineStageTiming last_stage_timing() const noexcept { return {}; }
};

}  // namespace p2dic
