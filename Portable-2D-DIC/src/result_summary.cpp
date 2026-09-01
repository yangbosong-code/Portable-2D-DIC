#include "p2dic/result_summary.hpp"

#include <cmath>

namespace p2dic {

ResultSummary summarize_result(const DicResult& result) noexcept {
    ResultSummary summary;
    summary.frame_sequence = result.frame_sequence;
    summary.frame_timestamp_ns = result.frame_timestamp_ns;
    summary.point_count = result.points.size();
    summary.processing_ms = result.processing_ms;
    for (const auto& point : result.points) {
        if (!point.valid || !std::isfinite(point.u) || !std::isfinite(point.v)) {
            continue;
        }
        ++summary.valid_count;
        summary.mean_u += point.u;
        summary.mean_v += point.v;
        if (point.strain_valid && std::isfinite(point.exx) && std::isfinite(point.eyy) &&
            std::isfinite(point.exy)) {
            ++summary.strain_valid_count;
            summary.mean_exx += point.exx;
            summary.mean_eyy += point.eyy;
            summary.mean_exy += point.exy;
        }
    }
    if (summary.point_count != 0) {
        summary.valid_ratio = static_cast<double>(summary.valid_count) /
                              static_cast<double>(summary.point_count);
    }
    if (summary.valid_count != 0) {
        summary.mean_u /= static_cast<double>(summary.valid_count);
        summary.mean_v /= static_cast<double>(summary.valid_count);
    }
    if (summary.point_count != 0) {
        summary.strain_valid_ratio = static_cast<double>(summary.strain_valid_count) /
                                     static_cast<double>(summary.point_count);
    }
    if (summary.strain_valid_count != 0) {
        summary.mean_exx /= static_cast<double>(summary.strain_valid_count);
        summary.mean_eyy /= static_cast<double>(summary.strain_valid_count);
        summary.mean_exy /= static_cast<double>(summary.strain_valid_count);
    }
    return summary;
}

}  // namespace p2dic
