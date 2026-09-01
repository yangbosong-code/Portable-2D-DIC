#pragma once

#include "p2dic/dic_engine.hpp"

namespace p2dic {

struct ZnccGridConfig {
    int subset_radius{20};
    int grid_step{32};
    int search_radius{8};
    double quality_threshold{0.80};
};

class ZnccGridEngine final : public IDicEngine {
public:
    explicit ZnccGridEngine(ZnccGridConfig config = {});
    [[nodiscard]] std::string_view name() const noexcept override;
    DicResult process(const Frame& reference, const Frame& deformed) override;

private:
    ZnccGridConfig config_;
};

}  // namespace p2dic
