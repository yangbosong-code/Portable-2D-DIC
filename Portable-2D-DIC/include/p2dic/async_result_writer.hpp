#pragma once

#include "p2dic/performance_stats.hpp"
#include "p2dic/result_writer.hpp"
#include "p2dic/session_binary.hpp"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace p2dic {

struct AsyncWriterConfig {
    std::size_t queue_capacity{64};
    std::size_t flush_every_frames{30};
    bool csv_enabled{false};
};

struct AsyncWriterMetrics {
    std::uint64_t enqueued{};
    std::uint64_t written{};
    std::uint64_t overruns{};
    std::size_t queue_depth{};
    std::size_t maximum_queue_depth{};
    bool faulted{false};
    double last_write_ms{};
    DistributionSnapshot write;
};

class AsyncResultWriter {
public:
    using FaultCallback = std::function<void(const std::string&)>;

    explicit AsyncResultWriter(AsyncWriterConfig config = {});
    ~AsyncResultWriter();
    AsyncResultWriter(const AsyncResultWriter&) = delete;
    AsyncResultWriter& operator=(const AsyncResultWriter&) = delete;

    void open(const std::filesystem::path& root, const SessionInfo& info);
    [[nodiscard]] bool enqueue(const DicResult& result);
    void close(std::string_view status = "complete");
    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] AsyncWriterMetrics metrics() const;
    void set_fault_callback(FaultCallback callback);

private:
    void worker_loop() noexcept;
    void report_fault(std::string message) noexcept;

    AsyncWriterConfig config_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<DicResult> queue_;
    std::unique_ptr<ResultWriter> manifest_and_csv_;
    SessionBinaryWriter binary_;
    std::thread worker_;
    bool open_{false};
    bool accepting_{false};
    bool stop_requested_{false};
    bool faulted_{false};
    std::string fault_message_;
    FaultCallback fault_callback_;
    std::uint64_t enqueued_{};
    std::uint64_t written_{};
    std::uint64_t overruns_{};
    std::size_t maximum_queue_depth_{};
    double last_write_ms_{};
    RollingStatistics write_statistics_;
};

}  // namespace p2dic
