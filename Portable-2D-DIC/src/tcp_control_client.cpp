#include "p2dic/tcp_control_client.hpp"

#include <array>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
using Socket = SOCKET;
constexpr Socket invalid_socket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
using Socket = int;
constexpr Socket invalid_socket = -1;
#endif

namespace p2dic {
namespace {

void close_socket(Socket socket) noexcept {
#ifdef _WIN32
    if (socket != invalid_socket) closesocket(socket);
#else
    if (socket != invalid_socket) close(socket);
#endif
}

}  // namespace

std::string send_control_command(
    std::string_view host,
    std::uint16_t port,
    std::string_view command,
    std::chrono::milliseconds timeout) {
    if (host.empty() || port == 0 || command.empty() || command.size() > 4096 ||
        command.find('\r') != std::string_view::npos || command.find('\n') != std::string_view::npos ||
        timeout.count() <= 0) {
        throw std::invalid_argument("Control client request is invalid");
    }
#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        throw std::runtime_error("Winsock startup failed");
    }
#endif
    Socket socket_handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_handle == invalid_socket) {
#ifdef _WIN32
        WSACleanup();
#endif
        throw std::runtime_error("Creating control socket failed");
    }
    const auto milliseconds = static_cast<int>(timeout.count());
#ifdef _WIN32
    setsockopt(socket_handle, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&milliseconds), sizeof(milliseconds));
    setsockopt(socket_handle, SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<const char*>(&milliseconds), sizeof(milliseconds));
#else
    timeval time_value{milliseconds / 1000, (milliseconds % 1000) * 1000};
    setsockopt(socket_handle, SOL_SOCKET, SO_RCVTIMEO, &time_value, sizeof(time_value));
    setsockopt(socket_handle, SOL_SOCKET, SO_SNDTIMEO, &time_value, sizeof(time_value));
#endif

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, std::string(host).c_str(), &address.sin_addr) != 1 ||
        connect(socket_handle, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
        close_socket(socket_handle);
#ifdef _WIN32
        WSACleanup();
#endif
        throw std::runtime_error("Connecting to DIC Edge failed");
    }
    std::string request(command);
    request += '\n';
    std::size_t sent = 0;
    while (sent < request.size()) {
        const int chunk = send(socket_handle, request.data() + sent,
                               static_cast<int>(request.size() - sent), 0);
        if (chunk <= 0) {
            close_socket(socket_handle);
#ifdef _WIN32
            WSACleanup();
#endif
            throw std::runtime_error("Sending control command failed");
        }
        sent += static_cast<std::size_t>(chunk);
    }

    std::string response;
    std::array<char, 1024> buffer{};
    while (response.size() <= 8192 && response.find('\n') == std::string::npos) {
        const int received = recv(socket_handle, buffer.data(), static_cast<int>(buffer.size()), 0);
        if (received <= 0) {
            break;
        }
        response.append(buffer.data(), static_cast<std::size_t>(received));
    }
    close_socket(socket_handle);
#ifdef _WIN32
    WSACleanup();
#endif
    const auto newline = response.find('\n');
    if (newline == std::string::npos) {
        throw std::runtime_error("DIC Edge returned an incomplete response");
    }
    response.resize(newline + 1);
    return response;
}

}  // namespace p2dic
