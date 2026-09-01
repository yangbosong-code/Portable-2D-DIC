#include "p2dic/tcp_control_server.hpp"

#include "p2dic/control_protocol.hpp"

#include <array>
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
    if (socket == invalid_socket) {
        return;
    }
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

bool send_all(Socket socket, std::string_view data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const auto remaining = data.size() - sent;
        const int chunk = send(socket, data.data() + sent, static_cast<int>(remaining), 0);
        if (chunk <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(chunk);
    }
    return true;
}

}  // namespace

TcpControlServer::TcpControlServer(Handler handler) : handler_(std::move(handler)) {
    if (!handler_) {
        throw std::invalid_argument("TCP control server requires a command handler");
    }
}

TcpControlServer::~TcpControlServer() {
    stop();
}

void TcpControlServer::start(std::uint16_t port, std::string bind_address) {
    if (port == 0 || bind_address.empty()) {
        throw std::invalid_argument("TCP control endpoint is invalid");
    }
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        throw std::logic_error("TCP control server is already running");
    }
    stop_requested_ = false;
    {
        std::lock_guard lock(startup_mutex_);
        startup_done_ = false;
        startup_succeeded_ = false;
        startup_error_.clear();
    }
    try {
        thread_ = std::thread(&TcpControlServer::run, this, port, std::move(bind_address));
    } catch (...) {
        running_ = false;
        throw;
    }
    std::unique_lock lock(startup_mutex_);
    if (!startup_condition_.wait_for(lock, std::chrono::seconds(2), [this] { return startup_done_; })) {
        lock.unlock();
        stop();
        throw std::runtime_error("TCP control server startup timed out");
    }
    if (!startup_succeeded_) {
        const std::string error = startup_error_;
        lock.unlock();
        if (thread_.joinable()) {
            thread_.join();
        }
        running_ = false;
        throw std::runtime_error(error);
    }
}

void TcpControlServer::stop() noexcept {
    stop_requested_ = true;
    if (thread_.joinable()) {
        thread_.join();
    }
    running_ = false;
}

bool TcpControlServer::running() const noexcept {
    return running_.load();
}

void TcpControlServer::run(std::uint16_t port, std::string bind_address) {
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
        signal_startup(false, "Winsock startup failed");
        return;
    }
#endif
    Socket listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == invalid_socket) {
#ifdef _WIN32
        WSACleanup();
#endif
        running_ = false;
        signal_startup(false, "Creating TCP listener failed");
        return;
    }
#ifdef _WIN32
    const BOOL exclusive = TRUE;
    if (setsockopt(listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                   reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) != 0) {
        close_socket(listener);
        WSACleanup();
        running_ = false;
        signal_startup(false, "Enabling exclusive TCP control endpoint failed");
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
        listen(listener, 8) != 0) {
        close_socket(listener);
#ifdef _WIN32
        WSACleanup();
#endif
        running_ = false;
        signal_startup(false, "Binding TCP control endpoint failed");
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
        if (selected <= 0) {
            continue;
        }
        Socket client = accept(listener, nullptr, nullptr);
        if (client == invalid_socket) {
            continue;
        }
        std::string buffer;
        std::array<char, 1024> chunk{};
        while (!stop_requested_ && buffer.size() <= 4096) {
            const int received = recv(client, chunk.data(), static_cast<int>(chunk.size()), 0);
            if (received <= 0) {
                break;
            }
            buffer.append(chunk.data(), static_cast<std::size_t>(received));
            const auto newline = buffer.find('\n');
            if (newline == std::string::npos) {
                continue;
            }
            std::string line = buffer.substr(0, newline);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            std::string response;
            try {
                response = handler_(line);
                if (response.empty() || response.back() != '\n') {
                    response = make_error_response("HANDLER", "Invalid empty handler response");
                }
            } catch (const std::exception& exception) {
                response = make_error_response("COMMAND", exception.what());
            } catch (...) {
                response = make_error_response("COMMAND", "Unknown command failure");
            }
            send_all(client, response);
            break;
        }
        if (buffer.size() > 4096) {
            send_all(client, make_error_response("TOO_LARGE", "Command exceeds 4096 bytes"));
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
