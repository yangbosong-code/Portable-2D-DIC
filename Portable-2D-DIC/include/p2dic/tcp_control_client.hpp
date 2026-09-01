#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

namespace p2dic {

std::string send_control_command(
    std::string_view host,
    std::uint16_t port,
    std::string_view command,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(2000));

}  // namespace p2dic
