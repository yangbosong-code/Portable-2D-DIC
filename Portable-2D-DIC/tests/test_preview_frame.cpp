#include "p2dic/preview_frame.hpp"
#include "p2dic/result_stream_client.hpp"
#include "p2dic/result_stream_server.hpp"

#include <iostream>
#include <stdexcept>

int main() {
    p2dic::Frame source(100, 80);
    source.sequence = 9;
    source.timestamp_ns = 1234;
    for (std::uint32_t y = 0; y < source.height; ++y) {
        for (std::uint32_t x = 0; x < source.width; ++x) {
            source.pixels[static_cast<std::size_t>(y) * source.stride + x] =
                static_cast<std::uint8_t>((x + y) & 0xFFU);
        }
    }
    try {
        const auto preview = p2dic::make_preview_frame(source, 50, 50);
        if (preview.width != 50 || preview.height != 40 || preview.sequence != 9 ||
            preview.pixels.front() != source.pixels.front()) {
            std::cerr << "Preview downsampling mismatch\n";
            return 1;
        }
        const auto packet = p2dic::encode_preview_packet(preview);
        const auto restored = p2dic::decode_preview_packet(packet);
        if (restored.width != preview.width || restored.height != preview.height ||
            restored.sequence != preview.sequence || restored.pixels != preview.pixels) {
            std::cerr << "Preview packet round-trip mismatch\n";
            return 2;
        }
        auto corrupted = packet;
        corrupted.back() ^= 1U;
        bool rejected = false;
        try {
            static_cast<void>(p2dic::decode_preview_packet(corrupted));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        if (!rejected) {
            std::cerr << "Corrupted preview packet was accepted\n";
            return 3;
        }
        p2dic::ResultStreamServer server;
        server.start(39852);
        server.publish_packet(packet);
        const auto network_packet = p2dic::receive_latest_packet(
            "127.0.0.1", 39852, 64U * 1024U * 1024U,
            std::chrono::milliseconds(2000));
        server.stop();
        const auto network_preview = p2dic::decode_preview_packet(network_packet);
        if (network_preview.pixels != preview.pixels) {
            std::cerr << "Preview TCP stream mismatch\n";
            return 4;
        }
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 5;
    }
    return 0;
}
