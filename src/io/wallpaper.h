#pragma once

#include <filesystem>
#include <optional>

namespace nenet {

[[nodiscard]] std::optional<std::filesystem::path> ensureWallpaper(const std::filesystem::path& cachePath,
                                                                     int maxAgeSeconds = 86400);

}
