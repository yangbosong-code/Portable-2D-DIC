#include "p2dic/edge_state.hpp"

#include <iostream>
#include <stdexcept>

int main() {
    p2dic::EdgeStateMachine machine;
    if (machine.state() != p2dic::EdgeState::booting) {
        return 1;
    }
    machine.boot_complete();
    machine.start_preview();
    machine.start_measurement();
    machine.pause_measurement();
    if (machine.state() != p2dic::EdgeState::paused) {
        return 2;
    }
    machine.resume_measurement();
    machine.stop_measurement();
    machine.fault();
    if (machine.state() != p2dic::EdgeState::faulted) {
        return 3;
    }

    bool rejected = false;
    try {
        machine.start_measurement();
    } catch (const std::logic_error&) {
        rejected = true;
    }
    if (!rejected) {
        std::cerr << "Measurement start should be rejected while faulted\n";
        return 4;
    }
    machine.reset_fault();
    machine.shutdown();
    machine.fault();
    return machine.state() == p2dic::EdgeState::shutting_down ? 0 : 5;
}
