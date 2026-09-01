#include "p2dic/session_binary.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: dic_export_csv INPUT.p2dic OUTPUT.csv\n";
        return 2;
    }
    try {
        const auto scan = p2dic::scan_session_binary(argv[1]);
        std::ofstream output(argv[2], std::ios::out | std::ios::trunc);
        if (!output) throw std::runtime_error("Unable to create output CSV");
        output << "frame_sequence,timestamp_ns,processing_ms,x,y,u,v,quality,valid,"
                  "exx,eyy,exy,strain_valid\n"
               << std::setprecision(9);
        for (const auto& result : scan.results) {
            for (const auto& point : result.points) {
                output << result.frame_sequence << ',' << result.frame_timestamp_ns << ','
                       << result.processing_ms << ',' << point.x << ',' << point.y << ','
                       << point.u << ',' << point.v << ',' << point.quality << ','
                       << (point.valid ? 1 : 0) << ',' << point.exx << ',' << point.eyy << ','
                       << point.exy << ',' << (point.strain_valid ? 1 : 0) << '\n';
            }
        }
        output.flush();
        if (!output) throw std::runtime_error("Writing output CSV failed");
        std::cout << "frames=" << scan.results.size()
                  << " recovered_bytes=" << scan.valid_bytes
                  << " truncated=" << (scan.truncated_or_corrupt ? 1 : 0) << '\n';
        return scan.truncated_or_corrupt ? 3 : 0;
    } catch (const std::exception& exception) {
        std::cerr << "Export failed: " << exception.what() << '\n';
        return 1;
    }
}
