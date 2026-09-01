#include "p2dic/session_binary.hpp"

#include "p2dic/result_packet.hpp"

#include <bit>
#include <limits>
#include <span>
#include <stdexcept>
#include <type_traits>

namespace p2dic {
namespace {

constexpr std::uint32_t file_magic = 0x31443250U;    // "P2D1"
constexpr std::uint32_t record_magic = 0x324D5246U;  // "FRM2"
constexpr std::uint32_t maximum_record_size = 64U * 1024U * 1024U;

template <typename UInt>
void append_le(std::vector<std::uint8_t>& output, UInt value) {
    static_assert(std::is_unsigned_v<UInt>);
    for (std::size_t index = 0; index < sizeof(UInt); ++index) {
        output.push_back(static_cast<std::uint8_t>(value >> (index * 8U)));
    }
}

template <typename UInt>
UInt read_le(std::span<const std::uint8_t> input, std::size_t& offset) {
    static_assert(std::is_unsigned_v<UInt>);
    if (offset > input.size() || input.size() - offset < sizeof(UInt)) {
        throw std::runtime_error("Binary session field is truncated");
    }
    UInt value = 0;
    for (std::size_t index = 0; index < sizeof(UInt); ++index) {
        value |= static_cast<UInt>(input[offset + index]) << (index * 8U);
    }
    offset += sizeof(UInt);
    return value;
}

std::uint32_t crc32(std::span<const std::uint8_t> data) noexcept {
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const auto value : data) {
        crc ^= value;
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

bool read_exact(std::ifstream& stream, std::vector<std::uint8_t>& output, std::size_t size) {
    output.resize(size);
    stream.read(reinterpret_cast<char*>(output.data()), static_cast<std::streamsize>(size));
    return static_cast<std::size_t>(stream.gcount()) == size;
}

}  // namespace

SessionBinaryWriter::~SessionBinaryWriter() { close(); }

void SessionBinaryWriter::open(const std::filesystem::path& session_path, const SessionInfo& info) {
    if (stream_.is_open()) throw std::logic_error("Binary session is already open");
    stream_.open(session_path / "results.p2dic", std::ios::binary | std::ios::trunc);
    if (!stream_) throw std::runtime_error("Unable to create results.p2dic");
    std::vector<std::uint8_t> header;
    header.reserve(session_binary_header_size);
    append_le(header, file_magic);
    append_le(header, session_binary_version);
    append_le(header, session_binary_header_size);
    append_le(header, info.width);
    append_le(header, info.height);
    append_le(header, std::bit_cast<std::uint64_t>(info.requested_fps));
    append_le(header, std::bit_cast<std::uint64_t>(info.millimeters_per_pixel));
    if (header.size() != session_binary_header_size) {
        throw std::logic_error("Binary session header size mismatch");
    }
    stream_.write(reinterpret_cast<const char*>(header.data()),
                  static_cast<std::streamsize>(header.size()));
    if (!stream_) throw std::runtime_error("Writing binary session header failed");
}

void SessionBinaryWriter::append(const DicResult& result) {
    if (!stream_.is_open()) throw std::logic_error("Binary session is not open");
    const auto packet = encode_result_packet(result);
    if (packet.size() > maximum_record_size - session_binary_record_header_size) {
        throw std::invalid_argument("Binary session result is too large");
    }
    std::vector<std::uint8_t> header;
    header.reserve(session_binary_record_header_size);
    append_le(header, record_magic);
    append_le(header, static_cast<std::uint32_t>(
                          session_binary_record_header_size + packet.size()));
    append_le(header, crc32(packet));
    append_le(header, 0U);
    stream_.write(reinterpret_cast<const char*>(header.data()),
                  static_cast<std::streamsize>(header.size()));
    stream_.write(reinterpret_cast<const char*>(packet.data()),
                  static_cast<std::streamsize>(packet.size()));
    if (!stream_) throw std::runtime_error("Writing binary session result failed");
}

void SessionBinaryWriter::flush() {
    if (!stream_.is_open()) return;
    stream_.flush();
    if (!stream_) throw std::runtime_error("Flushing binary session failed");
}

void SessionBinaryWriter::close() noexcept {
    if (stream_.is_open()) {
        stream_.flush();
        stream_.close();
    }
}

bool SessionBinaryWriter::is_open() const noexcept { return stream_.is_open(); }

SessionBinaryScan scan_session_binary(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Unable to open binary session: " + path.string());
    SessionBinaryScan scan;
    std::vector<std::uint8_t> bytes;
    if (!read_exact(stream, bytes, session_binary_header_size)) {
        throw std::runtime_error("Binary session header is truncated");
    }
    std::size_t offset = 0;
    const auto magic = read_le<std::uint32_t>(bytes, offset);
    const auto version = read_le<std::uint16_t>(bytes, offset);
    const auto header_size = read_le<std::uint16_t>(bytes, offset);
    scan.info.width = read_le<std::uint32_t>(bytes, offset);
    scan.info.height = read_le<std::uint32_t>(bytes, offset);
    scan.info.requested_fps = std::bit_cast<double>(read_le<std::uint64_t>(bytes, offset));
    scan.info.millimeters_per_pixel =
        std::bit_cast<double>(read_le<std::uint64_t>(bytes, offset));
    if (magic != file_magic || version != session_binary_version ||
        header_size != session_binary_header_size || scan.info.width == 0 ||
        scan.info.height == 0) {
        throw std::runtime_error("Binary session header is invalid or unsupported");
    }
    scan.valid_bytes = session_binary_header_size;
    while (true) {
        const auto record_begin = scan.valid_bytes;
        stream.peek();
        if (stream.eof()) break;
        if (!read_exact(stream, bytes, session_binary_record_header_size)) {
            scan.truncated_or_corrupt = true;
            break;
        }
        offset = 0;
        const auto current_magic = read_le<std::uint32_t>(bytes, offset);
        const auto record_size = read_le<std::uint32_t>(bytes, offset);
        const auto expected_crc = read_le<std::uint32_t>(bytes, offset);
        const auto reserved = read_le<std::uint32_t>(bytes, offset);
        if (current_magic != record_magic || reserved != 0U ||
            record_size < session_binary_record_header_size ||
            record_size > maximum_record_size) {
            scan.truncated_or_corrupt = true;
            break;
        }
        const auto payload_size = record_size - session_binary_record_header_size;
        if (!read_exact(stream, bytes, payload_size) || crc32(bytes) != expected_crc) {
            scan.truncated_or_corrupt = true;
            break;
        }
        try {
            scan.results.push_back(decode_result_packet(bytes));
        } catch (...) {
            scan.truncated_or_corrupt = true;
            break;
        }
        scan.valid_bytes = record_begin + record_size;
    }
    return scan;
}

}  // namespace p2dic
