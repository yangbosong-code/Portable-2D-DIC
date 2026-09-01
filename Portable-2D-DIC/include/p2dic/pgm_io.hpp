#pragma once

#include "p2dic/frame.hpp"

#include <filesystem>

namespace p2dic {

Frame load_pgm(const std::filesystem::path& path);
void save_pgm(const Frame& frame, const std::filesystem::path& path);

}  // namespace p2dic
