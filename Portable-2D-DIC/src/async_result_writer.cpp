#include "p2dic/async_result_writer.hpp"

#include <chrono>
#include <stdexcept>
#include <utility>

namespace p2dic {

AsyncResultWriter::AsyncResultWriter(AsyncWriterConfig config) : config_(config) {
    if (config_.queue_capacity == 0 || config_.flush_every_frames == 0) {
        throw std::invalid_argument("Async writer queue and flush interval must be positive");
    }
}

AsyncResultWriter::~AsyncResultWriter() {
    try {
        close("interrupted");
    } catch (...) {
    }
}

void AsyncResultWriter::open(const std::filesystem::path& root, const SessionInfo& info) {
    std::lock_guard lock(mutex_);
    if (open_) throw std::logic_error("An async result session is already open");
    manifest_and_csv_ = std::make_unique<ResultWriter>(
        config_.flush_every_frames, config_.csv_enabled);
    manifest_and_csv_->open(root, info);
    try {
        binary_.open(manifest_and_csv_->session_path(), info);
    } catch (...) {
        manifest_and_csv_->close("faulted");
        manifest_and_csv_.reset();
        throw;
    }
    queue_.clear();
    stop_requested_ = false;
    accepting_ = true;
    faulted_ = false;
    fault_message_.clear();
    enqueued_ = 0;
    written_ = 0;
    overruns_ = 0;
    maximum_queue_depth_ = 0;
    last_write_ms_ = 0.0;
    write_statistics_.clear();
    open_ = true;
    try {
        worker_ = std::thread(&AsyncResultWriter::worker_loop, this);
    } catch (...) {
        accepting_ = false;
        open_ = false;
        binary_.close();
        manifest_and_csv_->close("faulted");
        manifest_and_csv_.reset();
        throw;
    }
}

bool AsyncResultWriter::enqueue(const DicResult& result) {
    // Copy outside the lock: a dense result may be hundreds of kilobytes, and
    // the disk worker must remain able to drain the queue during that copy.
    DicResult copy = result;
    {
        std::lock_guard lock(mutex_);
        if (!open_ || !accepting_ || faulted_) {
            throw std::logic_error(fault_message_.empty()
                                       ? "Async result session is not accepting data"
                                       : fault_message_);
        }
        if (queue_.size() >= config_.queue_capacity) {
            ++overruns_;
            return false;
        }
        queue_.push_back(std::move(copy));
        ++enqueued_;
        maximum_queue_depth_ = std::max(maximum_queue_depth_, queue_.size());
    }
    condition_.notify_one();
    return true;
}

void AsyncResultWriter::close(std::string_view status) {
    {
        std::lock_guard lock(mutex_);
        if (!open_) return;
        accepting_ = false;
        stop_requested_ = true;
    }
    condition_.notify_all();
    if (worker_.joinable()) worker_.join();

    std::string worker_error;
    {
        std::lock_guard lock(mutex_);
        worker_error = fault_message_;
    }
    try {
        binary_.flush();
    } catch (const std::exception& exception) {
        if (worker_error.empty()) worker_error = exception.what();
    }
    binary_.close();
    const std::string final_status = worker_error.empty() ? std::string(status) : "faulted";
    if (manifest_and_csv_) {
        manifest_and_csv_->close(final_status);
        manifest_and_csv_.reset();
    }
    {
        std::lock_guard lock(mutex_);
        open_ = false;
        queue_.clear();
    }
    if (!worker_error.empty()) throw std::runtime_error(worker_error);
}

bool AsyncResultWriter::is_open() const noexcept {
    std::lock_guard lock(mutex_);
    return open_;
}

AsyncWriterMetrics AsyncResultWriter::metrics() const {
    std::lock_guard lock(mutex_);
    return AsyncWriterMetrics{
        enqueued_, written_, overruns_, queue_.size(), maximum_queue_depth_,
        faulted_, last_write_ms_, write_statistics_.snapshot()};
}

void AsyncResultWriter::set_fault_callback(FaultCallback callback) {
    std::lock_guard lock(mutex_);
    fault_callback_ = std::move(callback);
}

void AsyncResultWriter::worker_loop() noexcept {
    std::size_t since_flush = 0;
    try {
        while (true) {
            DicResult result;
            {
                std::unique_lock lock(mutex_);
                condition_.wait(lock, [&] { return stop_requested_ || !queue_.empty(); });
                if (queue_.empty() && stop_requested_) break;
                result = std::move(queue_.front());
                queue_.pop_front();
            }
            const auto start = std::chrono::steady_clock::now();
            binary_.append(result);
            manifest_and_csv_->append(result);
            if (++since_flush >= config_.flush_every_frames) {
                binary_.flush();
                since_flush = 0;
            }
            const double write_ms = std::chrono::duration<double, std::milli>(
                                        std::chrono::steady_clock::now() - start).count();
            {
                std::lock_guard lock(mutex_);
                ++written_;
                last_write_ms_ = write_ms;
            }
            write_statistics_.add(write_ms);
        }
    } catch (const std::exception& exception) {
        report_fault(std::string("Async result writer failed: ") + exception.what());
    } catch (...) {
        report_fault("Async result writer failed with an unknown error");
    }
}

void AsyncResultWriter::report_fault(std::string message) noexcept {
    FaultCallback callback;
    std::string delivered_message;
    {
        std::lock_guard lock(mutex_);
        faulted_ = true;
        accepting_ = false;
        fault_message_ = std::move(message);
        delivered_message = fault_message_;
        callback = fault_callback_;
    }
    if (callback) {
        try {
            callback(delivered_message);
        } catch (...) {
        }
    }
}

}  // namespace p2dic
