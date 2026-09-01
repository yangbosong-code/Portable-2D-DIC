#include "p2dic/result_stream_client.hpp"
#include "p2dic/result_stream_server.hpp"

#include <chrono>
#include <cmath>
#include <iostream>

int main() {
    using namespace std::chrono_literals;
    constexpr std::uint16_t port = 39850;
    p2dic::ResultStreamServer server;
    try {
        server.start(port);
        p2dic::ResultStreamServer duplicate;
        bool duplicate_rejected = false;
        try {
            duplicate.start(port);
        } catch (const std::exception&) {
            duplicate_rejected = true;
        }
        duplicate.stop();
        if (!duplicate_rejected) {
            server.stop();
            std::cerr << "A duplicate result endpoint was accepted\n";
            return 3;
        }
        p2dic::DicResult source;
        source.frame_sequence = 77;
        source.frame_timestamp_ns = 123456789;
        source.processing_ms = 3.75;
        source.points.push_back(p2dic::DicPoint{
            10, 20, 1.5F, -0.25F, 0.99F, true,
            0.001F, 0.002F, -0.0005F, true});
        server.publish(source);
        const auto restored = p2dic::receive_latest_result("127.0.0.1", port, 2s);
        server.stop();
        if (restored.frame_sequence != 77 || restored.points.size() != 1 ||
            std::abs(restored.points.front().u - 1.5F) > 1e-7F ||
            !restored.points.front().valid || !restored.points.front().strain_valid) {
            std::cerr << "TCP result stream round-trip mismatch\n";
            return 1;
        }
    } catch (const std::exception& exception) {
        server.stop();
        std::cerr << exception.what() << '\n';
        return 2;
    }
    return 0;
}
