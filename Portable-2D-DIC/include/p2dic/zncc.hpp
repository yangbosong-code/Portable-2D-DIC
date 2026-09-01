#pragma once

#include "p2dic/frame.hpp"

namespace p2dic {

struct TranslationResult {
    int dx{};
    int dy{};
    double score{-1.0};
};

TranslationResult find_integer_translation_zncc(
    const Frame& reference,
    const Frame& deformed,
    int center_x,
    int center_y,
    int subset_radius,
    int search_radius);

}  // namespace p2dic
