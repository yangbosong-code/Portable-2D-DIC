#pragma once

#include "p2dic/dic_engine.hpp"

#include <cstddef>
#include <cstdint>

namespace p2dic {

struct ResultSummary {
    std::uint64_t frame_sequence{};
    std::uint64_t frame_timestamp_ns{};
    std::size_t point_count{};
    std::size_t valid_count{};
    double valid_ratio{};
    double mean_u{};
    double mean_v{};
    std::size_t strain_valid_count{};
    double strain_valid_ratio{};
    double mean_exx{};
    double mean_eyy{};
    double mean_exy{};
    double processing_ms{};
};

[[nodiscard]] ResultSummary summarize_result(const DicResult& result) noexcept;

}  // namespace p2dic
