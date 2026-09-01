#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace p2dic {

struct ControlCommand {
    std::string verb;
    std::vector<std::string> arguments;
};

ControlCommand parse_control_command(std::string_view line);
std::string make_ok_response(std::string_view code, std::string_view fields = {});
std::string make_error_response(std::string_view code, std::string_view message);

}  // namespace p2dic
