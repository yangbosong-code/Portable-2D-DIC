#include "p2dic/strain_estimator.hpp"

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>

namespace p2dic {
namespace {

std::uint64_t coordinate_key(int x, int y) noexcept {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32U) |
           static_cast<std::uint32_t>(y);
}

}  // namespace

GridStrainEstimator::GridStrainEstimator(int grid_step) : grid_step_(grid_step) {
    if (grid_step_ < 1) {
        throw std::invalid_argument("Strain grid step must be positive");
    }
}

bool GridStrainEstimator::topology_matches(
    const std::vector<DicPoint>& points) const noexcept {
    if (coordinates_.size() != points.size()) {
        return false;
    }
    for (std::size_t index = 0; index < points.size(); ++index) {
        if (coordinates_[index].first != points[index].x ||
            coordinates_[index].second != points[index].y) {
            return false;
        }
    }
    return true;
}

void GridStrainEstimator::rebuild_topology(const std::vector<DicPoint>& points) {
    coordinates_.clear();
    coordinates_.reserve(points.size());
    neighbors_.assign(points.size(), NeighborSet{});
    std::unordered_map<std::uint64_t, int> lookup;
    lookup.reserve(points.size() * 2);
    for (std::size_t index = 0; index < points.size(); ++index) {
        coordinates_.emplace_back(points[index].x, points[index].y);
        lookup.emplace(coordinate_key(points[index].x, points[index].y),
                       static_cast<int>(index));
    }
    for (std::size_t index = 0; index < points.size(); ++index) {
        auto& set = neighbors_[index];
        for (int row = -1; row <= 1; ++row) {
            for (int column = -1; column <= 1; ++column) {
                if (row == 0 && column == 0) {
                    continue;
                }
                const auto found = lookup.find(coordinate_key(
                    points[index].x + column * grid_step_,
                    points[index].y + row * grid_step_));
                if (found != lookup.end()) {
                    set.indices[static_cast<std::size_t>(set.count++)] = found->second;
                }
            }
        }
    }
}

void GridStrainEstimator::apply(DicResult& result) {
    if (!topology_matches(result.points)) {
        rebuild_topology(result.points);
    }
    for (std::size_t index = 0; index < result.points.size(); ++index) {
        auto& center = result.points[index];
        center.exx = 0.0F;
        center.eyy = 0.0F;
        center.exy = 0.0F;
        center.strain_valid = false;
        if (!center.valid || !std::isfinite(center.u) || !std::isfinite(center.v)) {
            continue;
        }

        double a_xx = 0.0;
        double a_xy = 0.0;
        double a_yy = 0.0;
        double b_ux = 0.0;
        double b_uy = 0.0;
        double b_vx = 0.0;
        double b_vy = 0.0;
        int usable = 0;
        const auto& set = neighbors_[index];
        for (int neighbor_offset = 0; neighbor_offset < set.count; ++neighbor_offset) {
            const auto& neighbor = result.points[
                static_cast<std::size_t>(set.indices[static_cast<std::size_t>(neighbor_offset)])];
            if (!neighbor.valid || !std::isfinite(neighbor.u) || !std::isfinite(neighbor.v)) {
                continue;
            }
            const double dx = static_cast<double>(neighbor.x - center.x);
            const double dy = static_cast<double>(neighbor.y - center.y);
            const double du = static_cast<double>(neighbor.u - center.u);
            const double dv = static_cast<double>(neighbor.v - center.v);
            a_xx += dx * dx;
            a_xy += dx * dy;
            a_yy += dy * dy;
            b_ux += dx * du;
            b_uy += dy * du;
            b_vx += dx * dv;
            b_vy += dy * dv;
            ++usable;
        }
        const double determinant = a_xx * a_yy - a_xy * a_xy;
        if (usable < 3 || !std::isfinite(determinant) || std::abs(determinant) < 1.0e-12) {
            continue;
        }
        const double du_dx = (a_yy * b_ux - a_xy * b_uy) / determinant;
        const double du_dy = (-a_xy * b_ux + a_xx * b_uy) / determinant;
        const double dv_dx = (a_yy * b_vx - a_xy * b_vy) / determinant;
        const double dv_dy = (-a_xy * b_vx + a_xx * b_vy) / determinant;
        center.exx = static_cast<float>(du_dx);
        center.eyy = static_cast<float>(dv_dy);
        center.exy = static_cast<float>(0.5 * (du_dy + dv_dx));
        center.strain_valid = std::isfinite(center.exx) && std::isfinite(center.eyy) &&
                              std::isfinite(center.exy);
    }
}

}  // namespace p2dic
