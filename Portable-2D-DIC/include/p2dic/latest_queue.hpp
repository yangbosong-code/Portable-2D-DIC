#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>

namespace p2dic {

template <typename T>
struct QueuePop {
    T value;
    std::size_t skipped{};
};

template <typename T>
class LatestQueue {
public:
    explicit LatestQueue(std::size_t capacity) : capacity_(capacity) {
        if (capacity_ == 0) {
            throw std::invalid_argument("Queue capacity must be positive");
        }
    }

    LatestQueue(const LatestQueue&) = delete;
    LatestQueue& operator=(const LatestQueue&) = delete;

    std::size_t push(T item) {
        std::size_t dropped = 0;
        {
            std::lock_guard lock(mutex_);
            if (closed_) {
                return 0;
            }
            while (queue_.size() >= capacity_) {
                queue_.pop_front();
                ++dropped;
            }
            queue_.push_back(std::move(item));
        }
        condition_.notify_one();
        return dropped;
    }

    template <typename Rep, typename Period>
    std::optional<QueuePop<T>> wait_pop(const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock lock(mutex_);
        if (!condition_.wait_for(lock, timeout, [&] { return closed_ || !queue_.empty(); })) {
            return std::nullopt;
        }
        if (queue_.empty()) {
            return std::nullopt;
        }

        // Skip stale work intentionally. Real-time latency is more important than
        // processing every historical frame.
        const std::size_t skipped = queue_.size() - 1;
        T newest = std::move(queue_.back());
        queue_.clear();
        return QueuePop<T>{std::move(newest), skipped};
    }

    void reopen() {
        std::lock_guard lock(mutex_);
        queue_.clear();
        closed_ = false;
    }

    void close() {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
            queue_.clear();
        }
        condition_.notify_all();
    }

private:
    std::size_t capacity_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<T> queue_;
    bool closed_{false};
};

}  // namespace p2dic
