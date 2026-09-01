#include "p2dic/strain_estimator.hpp"

#include <cmath>
#include <iostream>

int main() {
    constexpr int step = 10;
    p2dic::DicResult result;
    for (int row = 0; row < 5; ++row) {
        for (int column = 0; column < 5; ++column) {
            const int x = column * step;
            const int y = row * step;
            const float u = 0.01F * x + 0.02F * y + 2.0F;
            const float v = -0.03F * x + 0.04F * y - 1.0F;
            result.points.push_back(p2dic::DicPoint{x, y, u, v, 1.0F, true});
        }
    }
    p2dic::GridStrainEstimator estimator(step);
    estimator.apply(result);
    for (const auto& point : result.points) {
        if (!point.strain_valid || std::abs(point.exx - 0.01F) > 1.0e-5F ||
            std::abs(point.eyy - 0.04F) > 1.0e-5F ||
            std::abs(point.exy + 0.005F) > 1.0e-5F) {
            std::cerr << "Strain mismatch at " << point.x << ',' << point.y << '\n';
            return 1;
        }
    }
    return 0;
}
