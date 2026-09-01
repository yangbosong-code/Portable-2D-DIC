#pragma once

#include "p2dic/frame.hpp"

namespace p2dic {

struct SubpixelConfig {
    int subset_radius{20};
    int max_iterations{30};
    double convergence_tolerance{1.0e-4};
    double minimum_texture_energy{1.0};
};

struct SubpixelTranslationResult {
    double u{};
    double v{};
    double zncc{-1.0};
    int iterations{};
    bool converged{false};
    bool valid{false};
};

// Forward-additive Gauss-Newton refinement on a zero-mean, unit-norm subset.
// The normalization makes the estimate insensitive to linear brightness changes.
SubpixelTranslationResult refine_translation_znssd(
    const Frame& reference,
    const Frame& deformed,
    int center_x,
    int center_y,
    double initial_u,
    double initial_v,
    const SubpixelConfig& config = {});

}  // namespace p2dic
