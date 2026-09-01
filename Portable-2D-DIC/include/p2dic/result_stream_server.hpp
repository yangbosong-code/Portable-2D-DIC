#pragma once

#include "p2dic/dic_engine.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace p2dic {

// Single-consumer, latest-frame TCP stream for DIC Studio. Slow clients do not
// queue historical frames: after each successful send the newest publication
// replaces any intermediate result.
class ResultStreamServer {
public:
    ResultStreamServer() = default;
    ~ResultStreamServer();
    ResultStreamServer(const ResultStreamServer&) = delete;
    ResultStreamServer& operator=(const ResultStreamServer&) = delete;

    void start(std::uint16_t port, std::string bind_address = "127.0.0.1");
    void stop() noexcept;
    void publish(const DicResult& result);
    void publish_packet(std::vector<std::uint8_t> packet);
    [[nodiscard]] bool running() const noexcept;

private:
    void run(std::uint16_t port, std::string bind_address);

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::mutex startup_mutex_;
    std::condition_variable startup_condition_;
    bool startup_done_{false};
    bool startup_succeeded_{false};
    std::string startup_error_;
    std::mutex packet_mutex_;
    std::condition_variable packet_condition_;
    std::shared_ptr<const std::vector<std::uint8_t>> latest_packet_;
    std::uint64_t generation_{0};
};

}  // namespace p2dic
