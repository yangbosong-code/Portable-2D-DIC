#include "p2dic/edge_state.hpp"

#include <stdexcept>
#include <string>

namespace p2dic {

EdgeState EdgeStateMachine::state() const noexcept {
    std::lock_guard lock(mutex_);
    return state_;
}

void EdgeStateMachine::require(EdgeState expected, std::string_view action) const {
    if (state_ != expected) {
        throw std::logic_error(
            std::string(action) + " is not allowed while edge state is " +
            std::string(to_string(state_)));
    }
}

void EdgeStateMachine::boot_complete() {
    std::lock_guard lock(mutex_);
    require(EdgeState::booting, "boot_complete");
    state_ = EdgeState::idle;
}

void EdgeStateMachine::start_preview() {
    std::lock_guard lock(mutex_);
    require(EdgeState::idle, "start_preview");
    state_ = EdgeState::previewing;
}

void EdgeStateMachine::stop_preview() {
    std::lock_guard lock(mutex_);
    require(EdgeState::previewing, "stop_preview");
    state_ = EdgeState::idle;
}

void EdgeStateMachine::start_measurement() {
    std::lock_guard lock(mutex_);
    if (state_ != EdgeState::idle && state_ != EdgeState::previewing) {
        throw std::logic_error(
            "start_measurement is not allowed while edge state is " +
            std::string(to_string(state_)));
    }
    state_ = EdgeState::measuring;
}

void EdgeStateMachine::stop_measurement() {
    std::lock_guard lock(mutex_);
    if (state_ != EdgeState::measuring && state_ != EdgeState::paused) {
        throw std::logic_error(
            "stop_measurement is not allowed while edge state is " +
            std::string(to_string(state_)));
    }
    state_ = EdgeState::idle;
}

void EdgeStateMachine::pause_measurement() {
    std::lock_guard lock(mutex_);
    require(EdgeState::measuring, "pause_measurement");
    state_ = EdgeState::paused;
}

void EdgeStateMachine::resume_measurement() {
    std::lock_guard lock(mutex_);
    require(EdgeState::paused, "resume_measurement");
    state_ = EdgeState::measuring;
}

void EdgeStateMachine::fault() {
    std::lock_guard lock(mutex_);
    if (state_ != EdgeState::shutting_down) {
        state_ = EdgeState::faulted;
    }
}

void EdgeStateMachine::reset_fault() {
    std::lock_guard lock(mutex_);
    require(EdgeState::faulted, "reset_fault");
    state_ = EdgeState::idle;
}

void EdgeStateMachine::shutdown() {
    std::lock_guard lock(mutex_);
    state_ = EdgeState::shutting_down;
}

std::string_view to_string(EdgeState state) noexcept {
    switch (state) {
        case EdgeState::booting:
            return "booting";
        case EdgeState::idle:
            return "idle";
        case EdgeState::previewing:
            return "previewing";
        case EdgeState::measuring:
            return "measuring";
        case EdgeState::paused:
            return "paused";
        case EdgeState::faulted:
            return "faulted";
        case EdgeState::shutting_down:
            return "shutting_down";
    }
    return "unknown";
}

}  // namespace p2dic
