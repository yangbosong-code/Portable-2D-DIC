#pragma once

#include "p2dic/dic_engine.hpp"
#include "p2dic/subpixel.hpp"

namespace p2dic {

struct HybridGridConfig {
    int subset_radius{20};
    int grid_step{32};
    int search_radius{8};
    int max_iterations{30};
    double convergence_tolerance{1.0e-4};
    double quality_threshold{0.80};
};

// Engineering CPU baseline: integer ZNCC initialization followed by
// photometrically normalized subpixel Gauss-Newton refinement.
class HybridGridEngine final : public IDicEngine {
public:
    explicit HybridGridEngine(HybridGridConfig config = {});
    [[nodiscard]] std::string_view name() const noexcept override;
    DicResult process(const Frame& reference, const Frame& deformed) override;

private:
    HybridGridConfig config_;
};

}  // namespace p2dic
