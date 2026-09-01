#include "p2dic/result_packet.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

bool close(float actual, float expected) {
    return std::abs(actual - expected) < 1e-7F;
}

}  // namespace

int main() {
    p2dic::DicResult source;
    source.frame_sequence = 42;
    source.frame_timestamp_ns = 9876543210ULL;
    source.processing_ms = 6.25;
    source.points = {
        p2dic::DicPoint{12, -4, 1.25F, -0.5F, 0.97F, true,
                        0.001F, -0.002F, 0.0003F, true},
        p2dic::DicPoint{44, 28, -3.0F, 2.0F, 0.25F, false,
                        0.0F, 0.0F, 0.0F, false}};

    try {
        const auto packet = p2dic::encode_result_packet(source);
        const auto restored = p2dic::decode_result_packet(packet);
        if (restored.frame_sequence != source.frame_sequence ||
            restored.frame_timestamp_ns != source.frame_timestamp_ns ||
            restored.processing_ms != source.processing_ms ||
            restored.points.size() != 2 ||
            restored.points[0].x != 12 || restored.points[0].y != -4 ||
            !close(restored.points[0].u, 1.25F) ||
            !close(restored.points[0].exy, 0.0003F) ||
            !restored.points[0].valid || !restored.points[0].strain_valid ||
            restored.points[1].valid || restored.points[1].strain_valid) {
            std::cerr << "DIC result packet round-trip mismatch\n";
            return 1;
        }

        auto corrupted = packet;
        corrupted.back() ^= 0x40U;
        bool checksum_rejected = false;
        try {
            static_cast<void>(p2dic::decode_result_packet(corrupted));
        } catch (const std::runtime_error&) {
            checksum_rejected = true;
        }
        bool truncation_rejected = false;
        try {
            static_cast<void>(p2dic::decode_result_packet(
                std::span<const std::uint8_t>(packet).first(packet.size() - 1)));
        } catch (const std::runtime_error&) {
            truncation_rejected = true;
        }
        if (!checksum_rejected || !truncation_rejected) {
            std::cerr << "Damaged DIC result packet was accepted\n";
            return 2;
        }
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 3;
    }
    return 0;
}
