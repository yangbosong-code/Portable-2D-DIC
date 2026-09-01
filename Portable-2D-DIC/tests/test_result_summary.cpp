#include "p2dic/result_summary.hpp"

#include <cmath>
#include <limits>

int main() {
    p2dic::DicResult result;
    result.frame_sequence = 42;
    result.frame_timestamp_ns = 123456;
    result.processing_ms = 7.5;
    result.points = {
        p2dic::DicPoint{0, 0, 1.0F, -1.0F, 0.99F, true, 0.01F, 0.02F, -0.01F, true},
        p2dic::DicPoint{1, 0, 3.0F, 1.0F, 0.98F, true, 0.03F, 0.04F, 0.01F, true},
        p2dic::DicPoint{2, 0, 99.0F, 99.0F, 0.10F, false},
        p2dic::DicPoint{3, 0, std::numeric_limits<float>::quiet_NaN(), 0.0F, 1.0F, true}};
    const auto summary = p2dic::summarize_result(result);
    return summary.frame_sequence == 42 && summary.frame_timestamp_ns == 123456 &&
                   summary.point_count == 4 && summary.valid_count == 2 &&
                   std::abs(summary.valid_ratio - 0.5) < 1.0e-12 &&
                   std::abs(summary.mean_u - 2.0) < 1.0e-12 &&
                   std::abs(summary.mean_v) < 1.0e-12 &&
                   summary.strain_valid_count == 2 &&
                   std::abs(summary.strain_valid_ratio - 0.5) < 1.0e-12 &&
                   std::abs(summary.mean_exx - 0.02) < 1.0e-6 &&
                   std::abs(summary.mean_eyy - 0.03) < 1.0e-6 &&
                   std::abs(summary.mean_exy) < 1.0e-6 &&
                   std::abs(summary.processing_ms - 7.5) < 1.0e-12
               ? 0
               : 1;
}
