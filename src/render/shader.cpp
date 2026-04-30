#include "shader.h"

#include <spdlog/spdlog.h>

#include <fstream>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace nenet {

std::filesystem::path executableDir() {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH];
    const DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        throw std::runtime_error("GetModuleFileName 失败");
    }
    return std::filesystem::path(buffer).parent_path();
#else
    return std::filesystem::canonical("/proc/self/exe").parent_path();
#endif
}

VkShaderModule loadShaderModule(VkDevice device, const std::filesystem::path& spvPath) {
    std::ifstream file(spvPath, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("无法打开 shader 文件: " + spvPath.string());
    }
    const std::streamsize size = file.tellg();
    if (size <= 0 || (size % 4) != 0) {
        throw std::runtime_error("shader 文件大小非 4 字节倍数: " + spvPath.string());
    }
    std::vector<uint32_t> code(static_cast<size_t>(size) / 4);
    file.seekg(0);
    file.read(reinterpret_cast<char*>(code.data()), size);

    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = static_cast<size_t>(size);
    info.pCode = code.data();

    VkShaderModule mod{VK_NULL_HANDLE};
    if (vkCreateShaderModule(device, &info, nullptr, &mod) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateShaderModule 失败: " + spvPath.string());
    }
    spdlog::info("已加载 shader: {} ({} 字节)", spvPath.filename().string(), size);
    return mod;
}

}
