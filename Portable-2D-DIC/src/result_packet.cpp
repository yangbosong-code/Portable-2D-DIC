#include "p2dic/result_packet.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace p2dic {
namespace {

constexpr std::uint32_t packet_magic = 0x32434944U;  // "DIC2" on the wire.
constexpr std::uint32_t maximum_point_count = 1'000'000U;

template <typename UInt>
void append_little_endian(std::vector<std::uint8_t>& output, UInt value) {
    static_assert(std::is_unsigned_v<UInt>);
    for (std::size_t byte = 0; byte < sizeof(UInt); ++byte) {
        output.push_back(static_cast<std::uint8_t>(value >> (byte * 8U)));
    }
}

void append_float(std::vector<std::uint8_t>& output, float value) {
    append_little_endian(output, std::bit_cast<std::uint32_t>(value));
}

void append_double(std::vector<std::uint8_t>& output, double value) {
    append_little_endian(output, std::bit_cast<std::uint64_t>(value));
}

template <typename UInt>
UInt read_little_endian(std::span<const std::uint8_t> input, std::size_t& offset) {
    static_assert(std::is_unsigned_v<UInt>);
    if (offset > input.size() || input.size() - offset < sizeof(UInt)) {
        throw std::runtime_error("DIC result packet is truncated");
    }
    UInt value = 0;
    for (std::size_t byte = 0; byte < sizeof(UInt); ++byte) {
        value |= static_cast<UInt>(input[offset + byte]) << (byte * 8U);
    }
    offset += sizeof(UInt);
    return value;
}

float read_float(std::span<const std::uint8_t> input, std::size_t& offset) {
    return std::bit_cast<float>(read_little_endian<std::uint32_t>(input, offset));
}

double read_double(std::span<const std::uint8_t> input, std::size_t& offset) {
    return std::bit_cast<double>(read_little_endian<std::uint64_t>(input, offset));
}

std::uint32_t crc32(std::span<const std::uint8_t> data) noexcept {
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const std::uint8_t value : data) {
        crc ^= value;
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

}  // namespace

std::vector<std::uint8_t> encode_result_packet(const DicResult& result) {
    if (result.points.size() > maximum_point_count) {
        throw std::invalid_argument("DIC result contains too many points");
    }
    constexpr std::size_t header_size = result_packet_header_size;
    constexpr std::size_t point_size = result_packet_point_size;
    const std::size_t payload_size = result.points.size() * point_size;
    const std::size_t packet_size = header_size + payload_size;
    if (packet_size > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("DIC result packet is too large");
    }

    std::vector<std::uint8_t> output;
    output.reserve(packet_size);
    // Reserve the header; the payload CRC is only known after point encoding.
    output.resize(header_size, 0);
    for (const auto& point : result.points) {
        append_little_endian(output, static_cast<std::uint32_t>(point.x));
        append_little_endian(output, static_cast<std::uint32_t>(point.y));
        append_float(output, point.u);
        append_float(output, point.v);
        append_float(output, point.quality);
        append_float(output, point.exx);
        append_float(output, point.eyy);
        append_float(output, point.exy);
        const std::uint32_t flags = (point.valid ? 1U : 0U) |
                                    (point.strain_valid ? 2U : 0U);
        append_little_endian(output, flags);
    }
    const std::uint32_t payload_crc = crc32(
        std::span<const std::uint8_t>(output).subspan(header_size));

    std::vector<std::uint8_t> header;
    header.reserve(header_size);
    append_little_endian(header, packet_magic);
    append_little_endian(header, result_packet_version);
    append_little_endian(header, result_packet_header_size);
    append_little_endian(header, static_cast<std::uint32_t>(packet_size));
    append_little_endian(header, result_packet_point_size);
    append_little_endian(header, result.frame_sequence);
    append_little_endian(header, result.frame_timestamp_ns);
    append_double(header, result.processing_ms);
    append_little_endian(header, static_cast<std::uint32_t>(result.points.size()));
    append_little_endian(header, payload_crc);
    if (header.size() != header_size) {
        throw std::logic_error("Internal DIC result header size mismatch");
    }
    std::copy(header.begin(), header.end(), output.begin());
    return output;
}

DicResult decode_result_packet(std::span<const std::uint8_t> packet) {
    if (packet.size() < result_packet_header_size) {
        throw std::runtime_error("DIC result packet is truncated");
    }
    std::size_t offset = 0;
    const auto magic = read_little_endian<std::uint32_t>(packet, offset);
    const auto version = read_little_endian<std::uint16_t>(packet, offset);
    const auto header_size = read_little_endian<std::uint16_t>(packet, offset);
    const auto declared_packet_size = read_little_endian<std::uint32_t>(packet, offset);
    const auto point_size = read_little_endian<std::uint32_t>(packet, offset);
    if (magic != packet_magic || version != result_packet_version ||
        header_size != result_packet_header_size || point_size != result_packet_point_size) {
        throw std::runtime_error("DIC result packet header is unsupported");
    }
    if (declared_packet_size != packet.size()) {
        throw std::runtime_error("DIC result packet length mismatch");
    }

    DicResult result;
    result.frame_sequence = read_little_endian<std::uint64_t>(packet, offset);
    result.frame_timestamp_ns = read_little_endian<std::uint64_t>(packet, offset);
    result.processing_ms = read_double(packet, offset);
    const auto point_count = read_little_endian<std::uint32_t>(packet, offset);
    const auto expected_crc = read_little_endian<std::uint32_t>(packet, offset);
    if (point_count > maximum_point_count ||
        packet.size() - result_packet_header_size !=
            static_cast<std::size_t>(point_count) * result_packet_point_size) {
        throw std::runtime_error("DIC result point count is invalid");
    }
    const auto payload = packet.subspan(result_packet_header_size);
    if (crc32(payload) != expected_crc) {
        throw std::runtime_error("DIC result packet checksum failed");
    }

    result.points.reserve(point_count);
    offset = result_packet_header_size;
    for (std::uint32_t index = 0; index < point_count; ++index) {
        DicPoint point;
        point.x = static_cast<std::int32_t>(read_little_endian<std::uint32_t>(packet, offset));
        point.y = static_cast<std::int32_t>(read_little_endian<std::uint32_t>(packet, offset));
        point.u = read_float(packet, offset);
        point.v = read_float(packet, offset);
        point.quality = read_float(packet, offset);
        point.exx = read_float(packet, offset);
        point.eyy = read_float(packet, offset);
        point.exy = read_float(packet, offset);
        const auto flags = read_little_endian<std::uint32_t>(packet, offset);
        if ((flags & ~3U) != 0U) {
            throw std::runtime_error("DIC result point flags are invalid");
        }
        point.valid = (flags & 1U) != 0U;
        point.strain_valid = (flags & 2U) != 0U;
        result.points.push_back(point);
    }
    return result;
}

}  // namespace p2dic
