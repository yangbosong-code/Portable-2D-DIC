#include "p2dic/tcp_control_client.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

std::unordered_map<std::string, std::string> parse_fields(std::string_view response) {
    constexpr std::string_view prefix = "OK STATUS ";
    if (!response.starts_with(prefix)) {
        throw std::runtime_error("Edge returned a non-STATUS response: " + std::string(response));
    }
    std::unordered_map<std::string, std::string> fields;
    std::size_t cursor = prefix.size();
    while (cursor < response.size()) {
        while (cursor < response.size() && response[cursor] == ' ') ++cursor;
        const auto end = response.find(' ', cursor);
        const auto token = response.substr(
            cursor, end == std::string_view::npos ? response.size() - cursor : end - cursor);
        const auto equal = token.find('=');
        if (equal != std::string_view::npos) {
            fields.emplace(std::string(token.substr(0, equal)), std::string(token.substr(equal + 1)));
        }
        if (end == std::string_view::npos) break;
        cursor = end + 1;
    }
    return fields;
}

std::string field(
    const std::unordered_map<std::string, std::string>& fields,
    const std::string& name) {
    const auto found = fields.find(name);
    return found == fields.end() ? std::string{} : found->second;
}

int positive_integer(const char* text, const char* name) {
    const int value = std::atoi(text);
    if (value <= 0) throw std::invalid_argument(std::string(name) + " must be positive");
    return value;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 5 || argc > 6) {
            std::cerr << "Usage: dic_perf_monitor HOST PORT DURATION_SECONDS OUTPUT.csv [INTERVAL_MS]\n";
            return 2;
        }
        const std::string host = argv[1];
        const auto port = static_cast<std::uint16_t>(positive_integer(argv[2], "port"));
        const int duration_seconds = positive_integer(argv[3], "duration");
        const std::string output_path = argv[4];
        const int interval_ms = argc == 6 ? positive_integer(argv[5], "interval") : 1000;
        const std::vector<std::string> columns{
            "state", "captured", "processed", "dropped", "timeouts",
            "pipeline_p50_ms", "pipeline_p95_ms", "pipeline_p99_ms", "e2e_p95_ms",
            "h2d_p95_ms", "kernel_p95_ms", "d2h_p95_ms", "strain_ms",
            "writer_queue", "writer_queue_max", "writer_overruns", "writer_p95_ms"};
        std::ofstream csv(output_path, std::ios::trunc);
        if (!csv) throw std::runtime_error("Unable to create performance CSV");
        csv << "elapsed_ms";
        for (const auto& column : columns) csv << ',' << column;
        csv << '\n';

        const auto start = std::chrono::steady_clock::now();
        const auto deadline = start + std::chrono::seconds(duration_seconds);
        std::unordered_map<std::string, std::string> latest;
        std::size_t samples = 0;
        while (std::chrono::steady_clock::now() < deadline) {
            const auto response = p2dic::send_control_command(host, port, "STATUS");
            latest = parse_fields(response);
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() - start).count();
            csv << elapsed;
            for (const auto& column : columns) csv << ',' << field(latest, column);
            csv << '\n';
            csv.flush();
            ++samples;
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
        }

        std::ofstream json(output_path + ".json", std::ios::trunc);
        if (!json) throw std::runtime_error("Unable to create performance JSON summary");
        json << "{\n  \"samples\": " << samples;
        for (const auto& column : columns) {
            json << ",\n  \"" << column << "\": \"" << field(latest, column) << '"';
        }
        json << "\n}\n";
        std::cout << "samples=" << samples << " csv=" << output_path
                  << " json=" << output_path << ".json\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Performance monitor failed: " << exception.what() << '\n';
        return 1;
    }
}
