#include "p2dic/control_protocol.hpp"
#include "p2dic/tcp_control_client.hpp"
#include "p2dic/tcp_control_server.hpp"

#include <iostream>

int main() {
    constexpr std::uint16_t port = 39847;
    p2dic::TcpControlServer server([](std::string_view line) {
        const auto command = p2dic::parse_control_command(line);
        return command.verb == "PING"
                   ? p2dic::make_ok_response("PONG", "version=1")
                   : p2dic::make_error_response("UNKNOWN", command.verb);
    });
    try {
        server.start(port);
        p2dic::TcpControlServer duplicate([](std::string_view) {
            return p2dic::make_ok_response("UNEXPECTED");
        });
        bool duplicate_rejected = false;
        try {
            duplicate.start(port);
        } catch (const std::exception&) {
            duplicate_rejected = true;
        }
        duplicate.stop();
        if (!duplicate_rejected) {
            server.stop();
            std::cerr << "A duplicate control endpoint was accepted\n";
            return 3;
        }
        const auto pong = p2dic::send_control_command("127.0.0.1", port, "PING");
        const auto unknown = p2dic::send_control_command("127.0.0.1", port, "NOPE");
        server.stop();
        if (pong != "OK PONG version=1\n" || unknown != "ERR UNKNOWN NOPE\n") {
            std::cerr << "Unexpected round-trip response\n";
            return 1;
        }
    } catch (const std::exception& exception) {
        server.stop();
        std::cerr << exception.what() << '\n';
        return 2;
    }
    return 0;
}
