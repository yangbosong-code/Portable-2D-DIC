#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <condition_variable>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

namespace p2dic {

class TcpControlServer {
public:
    using Handler = std::function<std::string(std::string_view)>;

    explicit TcpControlServer(Handler handler);
    ~TcpControlServer();
    TcpControlServer(const TcpControlServer&) = delete;
    TcpControlServer& operator=(const TcpControlServer&) = delete;

    void start(std::uint16_t port, std::string bind_address = "127.0.0.1");
    void stop() noexcept;
    [[nodiscard]] bool running() const noexcept;

private:
    void run(std::uint16_t port, std::string bind_address);
    Handler handler_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::mutex startup_mutex_;
    std::condition_variable startup_condition_;
    bool startup_done_{false};
    bool startup_succeeded_{false};
    std::string startup_error_;
};

}  // namespace p2dic
