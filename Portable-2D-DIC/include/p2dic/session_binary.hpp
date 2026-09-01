#pragma once

#include "p2dic/result_writer.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace p2dic {

inline constexpr std::uint16_t session_binary_version = 1;
inline constexpr std::uint16_t session_binary_header_size = 32;
inline constexpr std::uint32_t session_binary_record_header_size = 16;

struct SessionBinaryScan {
    SessionInfo info;
    std::vector<DicResult> results;
    std::uint64_t valid_bytes{};
    bool truncated_or_corrupt{false};
};

class SessionBinaryWriter {
public:
    SessionBinaryWriter() = default;
    ~SessionBinaryWriter();
    SessionBinaryWriter(const SessionBinaryWriter&) = delete;
    SessionBinaryWriter& operator=(const SessionBinaryWriter&) = delete;

    void open(const std::filesystem::path& session_path, const SessionInfo& info);
    void append(const DicResult& result);
    void flush();
    void close() noexcept;
    [[nodiscard]] bool is_open() const noexcept;

private:
    std::ofstream stream_;
};

[[nodiscard]] SessionBinaryScan scan_session_binary(const std::filesystem::path& path);

}  // namespace p2dic
