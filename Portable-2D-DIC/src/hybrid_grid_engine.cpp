#include "p2dic/hybrid_grid_engine.hpp"

#include "p2dic/zncc.hpp"

#include <chrono>
#include <stdexcept>

namespace p2dic {

HybridGridEngine::HybridGridEngine(HybridGridConfig config) : config_(config) {
    if (config_.subset_radius < 2 || config_.grid_step < 1 || config_.search_radius < 0 ||
        config_.max_iterations < 1 || config_.convergence_tolerance <= 0.0) {
        throw std::invalid_argument("Invalid hybrid DIC grid configuration");
    }
    if (config_.quality_threshold < -1.0 || config_.quality_threshold > 1.0) {
        throw std::invalid_argument("DIC quality threshold must be within [-1, 1]");
    }
}

std::string_view HybridGridEngine::name() const noexcept {
    return "CPU-ZNCC-ZNSSD-GN-Grid";
}

DicResult HybridGridEngine::process(const Frame& reference, const Frame& deformed) {
    if (reference.width != deformed.width || reference.height != deformed.height) {
        throw std::invalid_argument("Reference and deformed images must have equal dimensions");
    }
    const auto start = std::chrono::steady_clock::now();
    DicResult output;
    output.frame_sequence = deformed.sequence;
    output.frame_timestamp_ns = deformed.timestamp_ns;

    const int margin = config_.subset_radius + config_.search_radius + 1;
    const SubpixelConfig subpixel_config{
        config_.subset_radius,
        config_.max_iterations,
        config_.convergence_tolerance,
        1.0};
    for (int y = margin; y + margin < static_cast<int>(reference.height); y += config_.grid_step) {
        for (int x = margin; x + margin < static_cast<int>(reference.width); x += config_.grid_step) {
            const auto integer = find_integer_translation_zncc(
                reference,
                deformed,
                x,
                y,
                config_.subset_radius,
                config_.search_radius);
            const auto refined = refine_translation_znssd(
                reference,
                deformed,
                x,
                y,
                integer.dx,
                integer.dy,
                subpixel_config);
            const bool valid = refined.valid && refined.converged &&
                               refined.zncc >= config_.quality_threshold;
            output.points.push_back(DicPoint{
                x,
                y,
                static_cast<float>(refined.u),
                static_cast<float>(refined.v),
                static_cast<float>(refined.zncc),
                valid});
        }
    }

    output.processing_ms = std::chrono::duration<double, std::milli>(
                               std::chrono::steady_clock::now() - start)
                               .count();
    return output;
}

}  // namespace p2dic
