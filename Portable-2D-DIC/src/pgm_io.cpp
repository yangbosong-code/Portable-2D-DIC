#include "p2dic/pgm_io.hpp"

#include <cctype>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

namespace p2dic {
namespace {

std::string read_token(std::istream& input) {
    std::string token;
    char character = 0;
    while (input.get(character)) {
        if (character == '#') {
            input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        if (!std::isspace(static_cast<unsigned char>(character))) {
            token.push_back(character);
            break;
        }
    }
    while (input.get(character)) {
        if (std::isspace(static_cast<unsigned char>(character))) {
            break;
        }
        token.push_back(character);
    }
    if (token.empty()) {
        throw std::runtime_error("Unexpected end of PGM header");
    }
    return token;
}

}  // namespace

Frame load_pgm(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open PGM file: " + path.string());
    }
    if (read_token(input) != "P5") {
        throw std::runtime_error("Only binary P5 PGM files are supported");
    }
    const auto width = static_cast<std::uint32_t>(std::stoul(read_token(input)));
    const auto height = static_cast<std::uint32_t>(std::stoul(read_token(input)));
    const auto maximum = std::stoul(read_token(input));
    if (width == 0 || height == 0 || maximum != 255) {
        throw std::runtime_error("PGM must be non-empty 8-bit grayscale data");
    }

    Frame frame(width, height);
    input.read(
        reinterpret_cast<char*>(frame.pixels.data()),
        static_cast<std::streamsize>(frame.pixels.size()));
    if (input.gcount() != static_cast<std::streamsize>(frame.pixels.size())) {
        throw std::runtime_error("PGM pixel payload is truncated");
    }
    return frame;
}

void save_pgm(const Frame& frame, const std::filesystem::path& path) {
    if (frame.width == 0 || frame.height == 0 ||
        frame.pixels.size() < static_cast<std::size_t>(frame.stride) * frame.height) {
        throw std::invalid_argument("Cannot save an invalid frame");
    }
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Cannot create PGM file: " + path.string());
    }
    output << "P5\n" << frame.width << ' ' << frame.height << "\n255\n";
    if (frame.stride == frame.width) {
        output.write(
            reinterpret_cast<const char*>(frame.pixels.data()),
            static_cast<std::streamsize>(static_cast<std::size_t>(frame.width) * frame.height));
    } else {
        for (std::uint32_t y = 0; y < frame.height; ++y) {
            output.write(
                reinterpret_cast<const char*>(frame.pixels.data() + static_cast<std::size_t>(y) * frame.stride),
                frame.width);
        }
    }
    if (!output) {
        throw std::runtime_error("Failed while writing PGM file: " + path.string());
    }
}

}  // namespace p2dic
