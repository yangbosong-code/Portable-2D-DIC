#pragma once

#include "p2dic/dic_engine.hpp"

#include <memory>

namespace p2dic {

struct CudaGridConfig {
    int subset_radius{10};
    int grid_step{64};
    int max_iterations{15};
    float convergence_tolerance{1.0e-3F};
    float quality_threshold{0.80F};
    float maximum_iteration_step{1.0F};
    float recovery_trigger_valid_ratio{0.95F};
    int max_recovery_passes{1};
    bool inverse_compositional{false};
};

// Batched, temporally seeded translation Gauss-Newton engine. The first frame
// starts at zero displacement; following frames reuse the previous solution.
class CudaGridEngine final : public IDicEngine {
public:
    explicit CudaGridEngine(CudaGridConfig config = {});
    ~CudaGridEngine() override;
    CudaGridEngine(const CudaGridEngine&) = delete;
    CudaGridEngine& operator=(const CudaGridEngine&) = delete;

    [[nodiscard]] std::string_view name() const noexcept override;
    DicResult process(const Frame& reference, const Frame& deformed) override;
    [[nodiscard]] EngineStageTiming last_stage_timing() const noexcept override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace p2dic
