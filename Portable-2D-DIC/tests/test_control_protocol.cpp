#include "p2dic/control_protocol.hpp"

#include <iostream>
#include <stdexcept>

int main() {
    const auto command = p2dic::parse_control_command("measure session_id=test-001");
    if (command.verb != "MEASURE" || command.arguments.size() != 1 ||
        command.arguments.front() != "session_id=test-001") {
        std::cerr << "Valid command was parsed incorrectly\n";
        return 1;
    }
    bool rejected = false;
    try {
        static_cast<void>(p2dic::parse_control_command("PING\nSHUTDOWN"));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    if (!rejected) {
        std::cerr << "Embedded newline should have been rejected\n";
        return 2;
    }
    if (p2dic::make_ok_response("PONG") != "OK PONG\n" ||
        p2dic::make_error_response("BAD", "bad\nmessage") != "ERR BAD bad message\n") {
        return 3;
    }
    return 0;
}
