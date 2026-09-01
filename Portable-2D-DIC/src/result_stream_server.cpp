#include "p2dic/result_stream_server.hpp"

#include "p2dic/result_packet.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
using Socket = SOCKET;
constexpr Socket invalid_socket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
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

void set_send_timeout(Socket socket) noexcept {
#ifdef _WIN32
    const DWORD timeout_ms = 1000;
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
#else
    const timeval timeout{1, 0};
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif
}

bool send_all(Socket socket, const std::vector<std::uint8_t>& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const std::size_t remaining = data.size() - sent;
        const int request = static_cast<int>(
            std::min<std::size_t>(remaining, static_cast<std::size_t>(1U << 20U)));
        const int chunk = send(
            socket, reinterpret_cast<const char*>(data.data() + sent), request, 0);
        if (chunk <= 0) return false;
        sent += static_cast<std::size_t>(chunk);
    }
    return true;
}

}  // namespace

ResultStreamServer::~ResultStreamServer() {
    stop();
}

void ResultStreamServer::start(std::uint16_t port, std::string bind_address) {
    if (port == 0 || bind_address.empty()) {
        throw std::invalid_argument("TCP result stream endpoint is invalid");
    }
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        throw std::logic_error("TCP result stream server is already running");
    }
    stop_requested_ = false;
    {
        std::lock_guard lock(startup_mutex_);
        startup_done_ = false;
        startup_succeeded_ = false;
        startup_error_.clear();
    }
    try {
        thread_ = std::thread(&ResultStreamServer::run, this, port, std::move(bind_address));
    } catch (...) {
        running_ = false;
        throw;
    }
    std::unique_lock lock(startup_mutex_);
    if (!startup_condition_.wait_for(lock, std::chrono::seconds(2), [this] { return startup_done_; })) {
        lock.unlock();
        stop();
        throw std::runtime_error("TCP result stream startup timed out");
    }
    if (!startup_succeeded_) {
        const std::string error = startup_error_;
        lock.unlock();
        if (thread_.joinable()) thread_.join();
        running_ = false;
        throw std::runtime_error(error);
    }
}

void ResultStreamServer::stop() noexcept {
    stop_requested_ = true;
    packet_condition_.notify_all();
    if (thread_.joinable()) thread_.join();
    running_ = false;
}

void ResultStreamServer::publish(const DicResult& result) {
    publish_packet(encode_result_packet(result));
}

void ResultStreamServer::publish_packet(std::vector<std::uint8_t> packet_bytes) {
    if (packet_bytes.size() < 12) {
        throw std::invalid_argument("Published stream packet is too small");
    }
    const std::uint32_t declared_size =
        static_cast<std::uint32_t>(packet_bytes[8]) |
        (static_cast<std::uint32_t>(packet_bytes[9]) << 8U) |
        (static_cast<std::uint32_t>(packet_bytes[10]) << 16U) |
        (static_cast<std::uint32_t>(packet_bytes[11]) << 24U);
    if (declared_size != packet_bytes.size()) {
        throw std::invalid_argument("Published stream packet length is inconsistent");
    }
    auto packet = std::make_shared<const std::vector<std::uint8_t>>(
        std::move(packet_bytes));
    {
        std::lock_guard lock(packet_mutex_);
        latest_packet_ = std::move(packet);
        ++generation_;
    }
    packet_condition_.notify_one();
}

bool ResultStreamServer::running() const noexcept {
    return running_.load();
}

void ResultStreamServer::run(std::uint16_t port, std::string bind_address) {
    const auto signal_startup = [this](bool succeeded, std::string error = {}) {
        {
            std::lock_guard lock(startup_mutex_);
            startup_succeeded_ = succeeded;
            startup_error_ = std::move(error);
            startup_done_ = true;
        }
        startup_condition_.notify_all();
    };
#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        running_ = false;
        signal_startup(false, "Winsock startup failed for result stream");
        return;
    }
#endif
    Socket listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == invalid_socket) {
#ifdef _WIN32
        WSACleanup();
#endif
        running_ = false;
        signal_startup(false, "Creating TCP result listener failed");
        return;
    }
#ifdef _WIN32
    // SO_REUSEADDR on Windows permits two processes to bind the same endpoint,
    // which can split Studio connections between stale and current Edge instances.
    const BOOL exclusive = TRUE;
    if (setsockopt(listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                   reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) != 0) {
        close_socket(listener);
        WSACleanup();
        running_ = false;
        signal_startup(false, "Enabling exclusive TCP result endpoint failed");
        return;
    }
#else
    const int reuse = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, bind_address.c_str(), &address.sin_addr) != 1 ||
        bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0 ||
        listen(listener, 2) != 0) {
        close_socket(listener);
#ifdef _WIN32
        WSACleanup();
#endif
        running_ = false;
        signal_startup(false, "Binding TCP result stream endpoint failed");
        return;
    }
    signal_startup(true);

    while (!stop_requested_) {
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(listener, &read_set);
        timeval timeout{0, 200000};
#ifdef _WIN32
        const int selected = select(0, &read_set, nullptr, nullptr, &timeout);
#else
        const int selected = select(listener + 1, &read_set, nullptr, nullptr, &timeout);
#endif
        if (selected <= 0) continue;
        Socket client = accept(listener, nullptr, nullptr);
        if (client == invalid_socket) continue;
        set_send_timeout(client);
        std::uint64_t delivered_generation = 0;
        while (!stop_requested_) {
            std::shared_ptr<const std::vector<std::uint8_t>> packet;
            {
                std::unique_lock lock(packet_mutex_);
                packet_condition_.wait_for(lock, std::chrono::milliseconds(200), [&] {
                    return stop_requested_ || generation_ != delivered_generation;
                });
                if (stop_requested_) break;
                if (generation_ == delivered_generation || !latest_packet_) continue;
                delivered_generation = generation_;
                packet = latest_packet_;
            }
            if (!send_all(client, *packet)) break;
        }
        close_socket(client);
    }
    close_socket(listener);
#ifdef _WIN32
    WSACleanup();
#endif
    running_ = false;
}

}  // namespace p2dic
