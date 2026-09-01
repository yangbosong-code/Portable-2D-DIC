#include "p2dic/async_result_writer.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

int main() {
    const auto root = std::filesystem::temp_directory_path() / "p2dic_async_writer_test";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    try {
        p2dic::AsyncResultWriter writer(p2dic::AsyncWriterConfig{32, 4, false});
        writer.open(root, p2dic::SessionInfo{"async", "camera", 256, 256, 20.0, 0.0});
        for (std::uint64_t sequence = 1; sequence <= 20; ++sequence) {
            p2dic::DicResult result;
            result.frame_sequence = sequence;
            result.frame_timestamp_ns = sequence * 1'000'000ULL;
            result.processing_ms = 1.0;
            result.points.push_back(p2dic::DicPoint{10, 10, 1.0F, 0.0F, 1.0F, true});
            if (!writer.enqueue(result)) {
                std::cerr << "Async writer unexpectedly overran\n";
                return 1;
            }
        }
        writer.close("complete");
        const auto scan = p2dic::scan_session_binary(root / "async" / "results.p2dic");
        std::ifstream manifest(root / "async" / "manifest.json");
        std::ostringstream manifest_text;
        manifest_text << manifest.rdbuf();
        if (scan.results.size() != 20 || scan.results.back().frame_sequence != 20 ||
            scan.truncated_or_corrupt ||
            manifest_text.str().find("\"status\": \"complete\"") == std::string::npos ||
            std::filesystem::exists(root / "async" / "results.csv")) {
            std::cerr << "Async writer artifacts are incorrect\n";
            return 2;
        }
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 3;
    }
    std::filesystem::remove_all(root, error);
    return 0;
}
