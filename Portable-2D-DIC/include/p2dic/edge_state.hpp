#pragma once

#include <mutex>
#include <string_view>

namespace p2dic {

enum class EdgeState {
    booting,
    idle,
    previewing,
    measuring,
    paused,
    faulted,
    shutting_down,
};

class EdgeStateMachine {
public:
    EdgeStateMachine() = default;
    [[nodiscard]] EdgeState state() const noexcept;
    void boot_complete();
    void start_preview();
    void stop_preview();
    void start_measurement();
    void stop_measurement();
    void pause_measurement();
    void resume_measurement();
    void fault();
    void reset_fault();
    void shutdown();

private:
    void require(EdgeState expected, std::string_view action) const;
    mutable std::mutex mutex_;
    EdgeState state_{EdgeState::booting};
};

[[nodiscard]] std::string_view to_string(EdgeState state) noexcept;

}  // namespace p2dic
