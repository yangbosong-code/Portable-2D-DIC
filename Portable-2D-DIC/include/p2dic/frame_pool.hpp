#pragma once

#include "p2dic/frame.hpp"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <vector>

namespace p2dic {

class FramePool {
public:
    FramePool(std::size_t count, std::uint32_t width, std::uint32_t height)
        : state_(std::make_shared<State>()) {
        state_->owned.reserve(count);
        state_->available.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            auto frame = std::make_unique<Frame>(width, height);
            state_->available.push_back(frame.get());
            state_->owned.push_back(std::move(frame));
        }
    }

    FramePool(const FramePool&) = delete;
    FramePool& operator=(const FramePool&) = delete;

    ~FramePool() { close(); }

    std::shared_ptr<Frame> acquire(std::chrono::milliseconds timeout) {
        const auto state = state_;
        std::unique_lock lock(state->mutex);
        if (!state->condition.wait_for(lock, timeout, [&] {
                return state->closed || !state->available.empty();
            })) {
            return {};
        }
        if (state->closed || state->available.empty()) {
            return {};
        }
        Frame* frame = state->available.back();
        state->available.pop_back();
        return std::shared_ptr<Frame>(frame, [state](Frame* returned) {
            std::lock_guard guard(state->mutex);
            state->available.push_back(returned);
            state->condition.notify_one();
        });
    }

    void reopen() {
        const auto state = state_;
        std::lock_guard lock(state->mutex);
        if (state->available.size() != state->owned.size()) {
            throw std::logic_error("Cannot reopen frame pool while frames are still in use");
        }
        state->closed = false;
    }

    void close() {
        const auto state = state_;
        {
            std::lock_guard lock(state->mutex);
            state->closed = true;
        }
        state->condition.notify_all();
    }

private:
    struct State {
        std::mutex mutex;
        std::condition_variable condition;
        std::vector<std::unique_ptr<Frame>> owned;
        std::vector<Frame*> available;
        bool closed{false};
    };

    std::shared_ptr<State> state_;
};

}  // namespace p2dic
