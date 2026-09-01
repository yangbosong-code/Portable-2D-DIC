#include "p2dic/zncc_grid_engine.hpp"

#include "p2dic/zncc.hpp"

#include <chrono>
#include <stdexcept>

namespace p2dic {

ZnccGridEngine::ZnccGridEngine(ZnccGridConfig config) : config_(config) {
    if (config_.subset_radius < 2 || config_.grid_step < 1 || config_.search_radius < 0) {
        throw std::invalid_argument("Invalid ZNCC grid configuration");
    }
    if (config_.quality_threshold < -1.0 || config_.quality_threshold > 1.0) {
        throw std::invalid_argument("ZNCC threshold must be within [-1, 1]");
    }
}

std::string_view ZnccGridEngine::name() const noexcept {
    return "CPU-ZNCC-Grid";
}

DicResult ZnccGridEngine::process(const Frame& reference, const Frame& deformed) {
    const auto start = std::chrono::steady_clock::now();
    DicResult output;
    output.frame_sequence = deformed.sequence;
    output.frame_timestamp_ns = deformed.timestamp_ns;

    const int margin = config_.subset_radius + config_.search_radius;
    for (int y = margin; y + margin < static_cast<int>(reference.height); y += config_.grid_step) {
        for (int x = margin; x + margin < static_cast<int>(reference.width); x += config_.grid_step) {
            const auto match = find_integer_translation_zncc(
                reference,
                deformed,
                x,
                y,
                config_.subset_radius,
                config_.search_radius);
            output.points.push_back(DicPoint{
                x,
                y,
                static_cast<float>(match.dx),
                static_cast<float>(match.dy),
                static_cast<float>(match.score),
                match.score >= config_.quality_threshold});
        }
    }

    output.processing_ms = std::chrono::duration<double, std::milli>(
                               std::chrono::steady_clock::now() - start)
                               .count();
    return output;
}

}  // namespace p2dic
