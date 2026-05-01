#include "wallpaper.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdlib>
#include <string>
#include <system_error>

namespace nenet {

namespace {

constexpr const char* kApiUrl = "https://api.yppp.net/api.php";

bool fileFresh(const std::filesystem::path& p, int maxAgeSeconds) {
    std::error_code ec;
    if (!std::filesystem::exists(p, ec)) return false;
    const auto size = std::filesystem::file_size(p, ec);
    if (ec || size < 1024) return false;
    const auto ftime = std::filesystem::last_write_time(p, ec);
    if (ec) return false;
    const auto sysNow = decltype(ftime)::clock::now();
    const auto age = std::chrono::duration_cast<std::chrono::seconds>(sysNow - ftime).count();
    return age >= 0 && age < maxAgeSeconds;
}

bool downloadViaCurl(const std::filesystem::path& outPath) {
    const std::string outStr = outPath.string();
    std::string cmd = "curl -L -s --max-time 6 -A \"Mozilla/5.0\" -o \"";
    cmd += outStr;
    cmd += "\" \"";
    cmd += kApiUrl;
    cmd += "\"";
    spdlog::info("Wallpaper 下载: {}", cmd);
    const int rc = std::system(cmd.c_str());
    if (rc != 0) {
        spdlog::warn("curl 退出码 {}", rc);
        return false;
    }
    std::error_code ec;
    const auto sz = std::filesystem::file_size(outPath, ec);
    if (ec || sz < 1024) {
        spdlog::warn("壁纸大小异常 {} bytes", ec ? size_t{0} : sz);
        return false;
    }
    return true;
}

}

std::optional<std::filesystem::path> ensureWallpaper(const std::filesystem::path& cachePath,
                                                       int maxAgeSeconds) {
    if (fileFresh(cachePath, maxAgeSeconds)) {
        spdlog::info("Wallpaper 已缓存 {}", cachePath.string());
        return cachePath;
    }
    std::error_code ec;
    std::filesystem::create_directories(cachePath.parent_path(), ec);
    if (downloadViaCurl(cachePath)) {
        spdlog::info("Wallpaper 已下载 {}", cachePath.string());
        return cachePath;
    }
    if (std::filesystem::exists(cachePath, ec)) {
        spdlog::info("使用旧壁纸缓存 {}", cachePath.string());
        return cachePath;
    }
    return std::nullopt;
}

}
