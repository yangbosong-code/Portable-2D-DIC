#pragma once

#include "p2dic/dic_engine.hpp"

#include <array>
#include <cstddef>
#include <utility>
#include <vector>

namespace p2dic {

// Fits local displacement gradients from the 3x3 grid neighborhood and writes
// infinitesimal in-plane strains: exx=du/dx, eyy=dv/dy,
// exy=0.5*(du/dy+dv/dx). Coordinates and displacements use the same pixel scale.
class GridStrainEstimator {
public:
    explicit GridStrainEstimator(int grid_step);
    void apply(DicResult& result);

private:
    struct NeighborSet {
        std::array<int, 8> indices{};
        int count{};
    };

    void rebuild_topology(const std::vector<DicPoint>& points);
    [[nodiscard]] bool topology_matches(const std::vector<DicPoint>& points) const noexcept;

    int grid_step_{};
    std::vector<std::pair<int, int>> coordinates_;
    std::vector<NeighborSet> neighbors_;
};

}  // namespace p2dic
