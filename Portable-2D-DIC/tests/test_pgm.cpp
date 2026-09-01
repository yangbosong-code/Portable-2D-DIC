#include "p2dic/pgm_io.hpp"

#include <filesystem>
#include <iostream>

int main() {
    const auto path = std::filesystem::temp_directory_path() / "p2dic_pgm_roundtrip.pgm";
    p2dic::Frame source(17, 13);
    for (std::uint32_t y = 0; y < source.height; ++y) {
        for (std::uint32_t x = 0; x < source.width; ++x) {
            source.pixels[static_cast<std::size_t>(y) * source.stride + x] =
                static_cast<std::uint8_t>((x * 7 + y * 11) & 0xff);
        }
    }

    try {
        p2dic::save_pgm(source, path);
        const auto loaded = p2dic::load_pgm(path);
        std::filesystem::remove(path);
        if (loaded.width != source.width || loaded.height != source.height ||
            loaded.pixels != source.pixels) {
            std::cerr << "PGM round-trip content mismatch\n";
            return 1;
        }
    } catch (const std::exception& error) {
        std::filesystem::remove(path);
        std::cerr << error.what() << '\n';
        return 2;
    }
    return 0;
}
