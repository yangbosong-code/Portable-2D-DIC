#include "p2dic/preview_frame.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace p2dic {
namespace {

constexpr std::uint32_t preview_magic = 0x32474D49U;  // "IMG2" on the wire.
constexpr std::uint32_t maximum_dimension = 8192;

template <typename UInt>
void append_little_endian(std::vector<std::uint8_t>& output, UInt value) {
    static_assert(std::is_unsigned_v<UInt>);
    for (std::size_t byte = 0; byte < sizeof(UInt); ++byte) {
        output.push_back(static_cast<std::uint8_t>(value >> (byte * 8U)));
    }
}

template <typename UInt>
UInt read_little_endian(std::span<const std::uint8_t> input, std::size_t& offset) {
    static_assert(std::is_unsigned_v<UInt>);
    if (offset > input.size() || input.size() - offset < sizeof(UInt)) {
        throw std::runtime_error("Preview packet is truncated");
    }
    UInt value = 0;
    for (std::size_t byte = 0; byte < sizeof(UInt); ++byte) {
        value |= static_cast<UInt>(input[offset + byte]) << (byte * 8U);
    }
    offset += sizeof(UInt);
    return value;
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

PreviewFrame make_preview_frame(
    const Frame& source, std::uint32_t maximum_width, std::uint32_t maximum_height) {
    if (source.width == 0 || source.height == 0 || source.stride < source.width ||
        source.pixels.size() < static_cast<std::size_t>(source.stride) * source.height ||
        maximum_width == 0 || maximum_height == 0) {
        throw std::invalid_argument("Preview source or limits are invalid");
    }
    const double scale = std::min({
        1.0,
        static_cast<double>(maximum_width) / source.width,
        static_cast<double>(maximum_height) / source.height});
    PreviewFrame preview;
    preview.width = std::max(1U, static_cast<std::uint32_t>(std::floor(source.width * scale)));
    preview.height = std::max(1U, static_cast<std::uint32_t>(std::floor(source.height * scale)));
    preview.sequence = source.sequence;
    preview.timestamp_ns = source.timestamp_ns;
    preview.pixels.resize(static_cast<std::size_t>(preview.width) * preview.height);
    for (std::uint32_t y = 0; y < preview.height; ++y) {
        const std::uint32_t source_y = std::min(
            source.height - 1,
            static_cast<std::uint32_t>((static_cast<std::uint64_t>(y) * source.height) /
                                       preview.height));
        for (std::uint32_t x = 0; x < preview.width; ++x) {
            const std::uint32_t source_x = std::min(
                source.width - 1,
                static_cast<std::uint32_t>((static_cast<std::uint64_t>(x) * source.width) /
                                           preview.width));
            preview.pixels[static_cast<std::size_t>(y) * preview.width + x] =
                source.pixels[static_cast<std::size_t>(source_y) * source.stride + source_x];
        }
    }
    return preview;
}

std::vector<std::uint8_t> encode_preview_packet(const PreviewFrame& preview) {
    if (preview.width == 0 || preview.height == 0 ||
        preview.width > maximum_dimension || preview.height > maximum_dimension ||
        preview.pixels.size() != static_cast<std::size_t>(preview.width) * preview.height) {
        throw std::invalid_argument("Preview frame dimensions or pixels are invalid");
    }
    const std::size_t packet_size = preview_packet_header_size + preview.pixels.size();
    if (packet_size > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("Preview packet is too large");
    }
    std::vector<std::uint8_t> output;
    output.reserve(packet_size);
    append_little_endian(output, preview_magic);
    append_little_endian(output, preview_packet_version);
    append_little_endian(output, preview_packet_header_size);
    append_little_endian(output, static_cast<std::uint32_t>(packet_size));
    append_little_endian(output, preview.width);
    append_little_endian(output, preview.height);
    append_little_endian(output, preview.sequence);
    append_little_endian(output, preview.timestamp_ns);
    append_little_endian(output, crc32(preview.pixels));
    output.insert(output.end(), preview.pixels.begin(), preview.pixels.end());
    return output;
}

PreviewFrame decode_preview_packet(std::span<const std::uint8_t> packet) {
    if (packet.size() < preview_packet_header_size) {
        throw std::runtime_error("Preview packet is truncated");
    }
    std::size_t offset = 0;
    const auto magic = read_little_endian<std::uint32_t>(packet, offset);
    const auto version = read_little_endian<std::uint16_t>(packet, offset);
    const auto header_size = read_little_endian<std::uint16_t>(packet, offset);
    const auto packet_size = read_little_endian<std::uint32_t>(packet, offset);
    PreviewFrame preview;
    preview.width = read_little_endian<std::uint32_t>(packet, offset);
    preview.height = read_little_endian<std::uint32_t>(packet, offset);
    preview.sequence = read_little_endian<std::uint64_t>(packet, offset);
    preview.timestamp_ns = read_little_endian<std::uint64_t>(packet, offset);
    const auto expected_crc = read_little_endian<std::uint32_t>(packet, offset);
    if (magic != preview_magic || version != preview_packet_version ||
        header_size != preview_packet_header_size || packet_size != packet.size() ||
        preview.width == 0 || preview.height == 0 ||
        preview.width > maximum_dimension || preview.height > maximum_dimension ||
        packet.size() - preview_packet_header_size !=
            static_cast<std::size_t>(preview.width) * preview.height) {
        throw std::runtime_error("Preview packet header is invalid");
    }
    const auto pixels = packet.subspan(preview_packet_header_size);
    if (crc32(pixels) != expected_crc) {
        throw std::runtime_error("Preview packet checksum failed");
    }
    preview.pixels.assign(pixels.begin(), pixels.end());
    return preview;
}

}  // namespace p2dic
