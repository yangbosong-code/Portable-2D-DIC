#include "p2dic/session_binary.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

p2dic::DicResult make_result(std::uint64_t sequence) {
    p2dic::DicResult result;
    result.frame_sequence = sequence;
    result.frame_timestamp_ns = sequence * 1'000'000ULL;
    result.processing_ms = 2.5;
    result.points.push_back(p2dic::DicPoint{
        10, 20, 1.25F, -0.5F, 0.98F, true, 0.001F, -0.002F, 0.0005F, true});
    return result;
}

}  // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "p2dic_binary_session_test";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root);
    const auto path = root / "results.p2dic";
    try {
        p2dic::SessionBinaryWriter writer;
        writer.open(root, p2dic::SessionInfo{"binary", "camera", 512, 512, 20.0, 0.01});
        writer.append(make_result(1));
        writer.append(make_result(2));
        writer.append(make_result(3));
        writer.flush();
        writer.close();
        const auto complete = p2dic::scan_session_binary(path);
        if (complete.truncated_or_corrupt || complete.results.size() != 3 ||
            complete.results.back().frame_sequence != 3 ||
            complete.info.millimeters_per_pixel != 0.01) {
            std::cerr << "Complete binary session scan failed\n";
            return 1;
        }
        const auto truncated_path = root / "truncated.p2dic";
        std::filesystem::copy_file(path, truncated_path);
        const auto size = std::filesystem::file_size(truncated_path);
        std::filesystem::resize_file(truncated_path, size - 5);
        const auto truncated = p2dic::scan_session_binary(truncated_path);
        if (!truncated.truncated_or_corrupt || truncated.results.size() != 2 ||
            truncated.valid_bytes <= p2dic::session_binary_header_size) {
            std::cerr << "Truncated binary session recovery failed\n";
            return 2;
        }
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 3;
    }
    std::filesystem::remove_all(root, error);
    return 0;
}
