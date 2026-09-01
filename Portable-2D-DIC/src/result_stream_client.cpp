#include "p2dic/result_stream_client.hpp"

#include "p2dic/result_packet.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <vector>

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
    if (socket == invalid_socket) return;
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

class SocketHandle {
public:
    explicit SocketHandle(Socket value) : value_(value) {}
    ~SocketHandle() { close_socket(value_); }
    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;
    [[nodiscard]] Socket get() const noexcept { return value_; }
private:
    Socket value_;
};

#ifdef _WIN32
class WinsockSession {
public:
    WinsockSession() {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            throw std::runtime_error("Winsock startup failed for result client");
        }
    }
    ~WinsockSession() { WSACleanup(); }
};
#endif

void set_socket_timeout(Socket socket, std::chrono::milliseconds timeout) {
    if (timeout.count() <= 0) {
        throw std::invalid_argument("Result receive timeout must be positive");
    }
#ifdef _WIN32
    const DWORD timeout_ms = static_cast<DWORD>(timeout.count());
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
#else
    const timeval time_value{
        static_cast<time_t>(timeout.count() / 1000),
        static_cast<suseconds_t>((timeout.count() % 1000) * 1000)};
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &time_value, sizeof(time_value));
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &time_value, sizeof(time_value));
#endif
}

void receive_exact(Socket socket, std::uint8_t* destination, std::size_t size) {
    std::size_t received_total = 0;
    while (received_total < size) {
        const std::size_t remaining = size - received_total;
        const int request = static_cast<int>(
            std::min<std::size_t>(remaining, static_cast<std::size_t>(1U << 20U)));
        const int received = recv(
            socket, reinterpret_cast<char*>(destination + received_total), request, 0);
        if (received <= 0) {
            throw std::runtime_error("Receiving DIC result stream timed out or disconnected");
        }
        received_total += static_cast<std::size_t>(received);
    }
}

std::uint32_t packet_size_from_header(const std::vector<std::uint8_t>& header) {
    return static_cast<std::uint32_t>(header[8]) |
           (static_cast<std::uint32_t>(header[9]) << 8U) |
           (static_cast<std::uint32_t>(header[10]) << 16U) |
           (static_cast<std::uint32_t>(header[11]) << 24U);
}

}  // namespace

std::vector<std::uint8_t> receive_latest_packet(
    const std::string& host,
    std::uint16_t port,
    std::uint32_t maximum_packet_size,
    std::chrono::milliseconds timeout) {
    if (host.empty() || port == 0 || maximum_packet_size < 12) {
        throw std::invalid_argument("Result stream endpoint is invalid");
    }
#ifdef _WIN32
    WinsockSession winsock;
#endif
    SocketHandle socket_handle(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (socket_handle.get() == invalid_socket) {
        throw std::runtime_error("Creating result stream socket failed");
    }
    set_socket_timeout(socket_handle.get(), timeout);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1 ||
        connect(socket_handle.get(), reinterpret_cast<const sockaddr*>(&address),
                sizeof(address)) != 0) {
        throw std::runtime_error("Connecting to DIC result stream failed");
    }

    std::vector<std::uint8_t> packet(12);
    receive_exact(socket_handle.get(), packet.data(), packet.size());
    const std::uint32_t packet_size = packet_size_from_header(packet);
    if (packet_size < packet.size() || packet_size > maximum_packet_size) {
        throw std::runtime_error("DIC result stream declared an invalid packet size");
    }
    const std::size_t header_size = packet.size();
    packet.resize(packet_size);
    receive_exact(socket_handle.get(), packet.data() + header_size, packet.size() - header_size);
    return packet;
}

DicResult receive_latest_result(
    const std::string& host,
    std::uint16_t port,
    std::chrono::milliseconds timeout) {
    constexpr std::uint32_t maximum_packet_size =
        result_packet_header_size + 1'000'000U * result_packet_point_size;
    return decode_result_packet(
        receive_latest_packet(host, port, maximum_packet_size, timeout));
}

}  // namespace p2dic
