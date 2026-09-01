#include "p2dic/result_writer.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

int main() {
    const auto root = std::filesystem::temp_directory_path() / "p2dic_result_writer_test";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    try {
        p2dic::ResultWriter writer(1);
        writer.open(root, p2dic::SessionInfo{
            "session-001", "SyntheticCamera", 512, 512, 20.0, 0.0125});
        p2dic::DicResult result;
        result.frame_sequence = 7;
        result.frame_timestamp_ns = 123456;
        result.processing_ms = 4.25;
        result.points.push_back(
            p2dic::DicPoint{10, 20, 1.25F, -0.5F, 0.98F, true,
                            0.001F, -0.002F, 0.0005F, true});
        writer.append(result);
        writer.close("complete");

        std::ifstream manifest(root / "session-001" / "manifest.json");
        std::ifstream csv(root / "session-001" / "results.csv");
        std::ostringstream manifest_text;
        std::ostringstream csv_text;
        manifest_text << manifest.rdbuf();
        csv_text << csv.rdbuf();
        if (manifest_text.str().find("\"status\": \"complete\"") == std::string::npos ||
            manifest_text.str().find("\"millimeters_per_pixel\": 0.0125") == std::string::npos ||
            csv_text.str().find("exx,eyy,exy,strain_valid") == std::string::npos ||
            csv_text.str().find("7,123456,4.25,10,20,1.25,-0.5,") ==
                std::string::npos ||
            csv_text.str().rfind(",1") == std::string::npos) {
            std::cerr << "Session artifacts are incomplete\n";
            std::filesystem::remove_all(root, error);
            return 1;
        }
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        std::filesystem::remove_all(root, error);
        return 2;
    }
    std::filesystem::remove_all(root, error);
    return 0;
}
