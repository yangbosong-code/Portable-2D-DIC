#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <mutex>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace p2dic {

struct DistributionSnapshot {
    std::size_t sample_count{};
    double mean{};
    double p50{};
    double p95{};
    double p99{};
    double maximum{};
};

class RollingStatistics {
public:
    explicit RollingStatistics(std::size_t capacity = 1024) : capacity_(capacity) {
        if (capacity_ == 0) throw std::invalid_argument("Statistics capacity must be positive");
    }

    void add(double value) {
        if (!std::isfinite(value) || value < 0.0) return;
        std::lock_guard lock(mutex_);
        if (values_.size() == capacity_) values_.pop_front();
        values_.push_back(value);
    }

    [[nodiscard]] DistributionSnapshot snapshot() const {
        std::vector<double> sorted;
        {
            std::lock_guard lock(mutex_);
            sorted.assign(values_.begin(), values_.end());
        }
        DistributionSnapshot output;
        output.sample_count = sorted.size();
        if (sorted.empty()) return output;
        output.mean = std::accumulate(sorted.begin(), sorted.end(), 0.0) /
                      static_cast<double>(sorted.size());
        std::sort(sorted.begin(), sorted.end());
        const auto percentile = [&sorted](double fraction) {
            const auto rank = static_cast<std::size_t>(
                std::ceil(fraction * static_cast<double>(sorted.size())));
            return sorted[std::min(sorted.size() - 1, std::max<std::size_t>(1, rank) - 1)];
        };
        output.p50 = percentile(0.50);
        output.p95 = percentile(0.95);
        output.p99 = percentile(0.99);
        output.maximum = sorted.back();
        return output;
    }

    void clear() {
        std::lock_guard lock(mutex_);
        values_.clear();
    }

private:
    std::size_t capacity_;
    mutable std::mutex mutex_;
    std::deque<double> values_;
};

}  // namespace p2dic
