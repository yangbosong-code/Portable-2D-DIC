#include "p2dic/zncc.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace p2dic {

TranslationResult find_integer_translation_zncc(
    const Frame& reference,
    const Frame& deformed,
    int center_x,
    int center_y,
    int subset_radius,
    int search_radius) {
    if (reference.width != deformed.width || reference.height != deformed.height) {
        throw std::invalid_argument("Reference and deformed images must have equal dimensions");
    }
    if (subset_radius < 1 || search_radius < 0) {
        throw std::invalid_argument("Subset and search radii are invalid");
    }

    const int margin = subset_radius + search_radius;
    if (center_x < margin || center_y < margin ||
        center_x + margin >= static_cast<int>(reference.width) ||
        center_y + margin >= static_cast<int>(reference.height)) {
        throw std::invalid_argument("Subset and search window exceed the image boundary");
    }

    const int side = 2 * subset_radius + 1;
    const int sample_count = side * side;
    double reference_sum = 0.0;

    for (int sy = -subset_radius; sy <= subset_radius; ++sy) {
        for (int sx = -subset_radius; sx <= subset_radius; ++sx) {
            reference_sum += reference.at(
                static_cast<std::uint32_t>(center_x + sx),
                static_cast<std::uint32_t>(center_y + sy));
        }
    }
    const double reference_mean = reference_sum / sample_count;

    double reference_energy = 0.0;
    for (int sy = -subset_radius; sy <= subset_radius; ++sy) {
        for (int sx = -subset_radius; sx <= subset_radius; ++sx) {
            const double value = reference.at(
                                     static_cast<std::uint32_t>(center_x + sx),
                                     static_cast<std::uint32_t>(center_y + sy)) -
                                 reference_mean;
            reference_energy += value * value;
        }
    }

    TranslationResult best;
    for (int dy = -search_radius; dy <= search_radius; ++dy) {
        for (int dx = -search_radius; dx <= search_radius; ++dx) {
            double deformed_sum = 0.0;
            for (int sy = -subset_radius; sy <= subset_radius; ++sy) {
                for (int sx = -subset_radius; sx <= subset_radius; ++sx) {
                    deformed_sum += deformed.at(
                        static_cast<std::uint32_t>(center_x + sx + dx),
                        static_cast<std::uint32_t>(center_y + sy + dy));
                }
            }
            const double deformed_mean = deformed_sum / sample_count;

            double numerator = 0.0;
            double deformed_energy = 0.0;
            for (int sy = -subset_radius; sy <= subset_radius; ++sy) {
                for (int sx = -subset_radius; sx <= subset_radius; ++sx) {
                    const double a = reference.at(
                                         static_cast<std::uint32_t>(center_x + sx),
                                         static_cast<std::uint32_t>(center_y + sy)) -
                                     reference_mean;
                    const double b = deformed.at(
                                         static_cast<std::uint32_t>(center_x + sx + dx),
                                         static_cast<std::uint32_t>(center_y + sy + dy)) -
                                     deformed_mean;
                    numerator += a * b;
                    deformed_energy += b * b;
                }
            }

            const double denominator = std::sqrt(reference_energy * deformed_energy);
            const double score = denominator > std::numeric_limits<double>::epsilon()
                                     ? numerator / denominator
                                     : -1.0;
            if (score > best.score) {
                best = TranslationResult{dx, dy, score};
            }
        }
    }
    return best;
}

}  // namespace p2dic
