#include "p2dic/tcp_control_client.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2 || argc > 4) {
        std::cerr << "Usage: dic_ctl COMMAND [HOST] [PORT]\n";
        return 2;
    }
    const std::string host = argc >= 3 ? argv[2] : "127.0.0.1";
    const auto port = static_cast<std::uint16_t>(argc >= 4 ? std::stoul(argv[3]) : 17840);
    try {
        std::cout << p2dic::send_control_command(host, port, argv[1]);
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
