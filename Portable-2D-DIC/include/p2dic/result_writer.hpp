#pragma once

#include "p2dic/dic_engine.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>

namespace p2dic {

struct SessionInfo {
    std::string session_id;
    std::string camera_name;
    std::uint32_t width{};
    std::uint32_t height{};
    double requested_fps{};
    double millimeters_per_pixel{};
};

class ResultWriter {
public:
    explicit ResultWriter(std::size_t flush_every_frames = 30, bool csv_enabled = true);
    ~ResultWriter();
    ResultWriter(const ResultWriter&) = delete;
    ResultWriter& operator=(const ResultWriter&) = delete;

    void open(const std::filesystem::path& root, const SessionInfo& info);
    void append(const DicResult& result);
    void close(std::string_view status = "complete");
    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] const std::filesystem::path& session_path() const noexcept;

private:
    void write_manifest(const SessionInfo& info, std::string_view status);
    std::size_t flush_every_frames_;
    bool csv_enabled_{true};
    bool session_open_{false};
    std::size_t frames_since_flush_{};
    std::filesystem::path session_path_;
    SessionInfo info_;
    std::ofstream results_;
};

}  // namespace p2dic
