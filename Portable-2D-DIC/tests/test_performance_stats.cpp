#include "p2dic/performance_stats.hpp"

#include <cmath>
#include <iostream>

int main() {
    p2dic::RollingStatistics statistics(5);
    for (int value = 1; value <= 10; ++value) statistics.add(static_cast<double>(value));
    statistics.add(-1.0);
    const auto summary = statistics.snapshot();
    if (summary.sample_count != 5 || std::abs(summary.mean - 8.0) > 1e-12 ||
        summary.p50 != 8.0 || summary.p95 != 10.0 || summary.p99 != 10.0 ||
        summary.maximum != 10.0) {
        std::cerr << "Rolling statistics result is incorrect\n";
        return 1;
    }
    statistics.clear();
    return statistics.snapshot().sample_count == 0 ? 0 : 2;
}
