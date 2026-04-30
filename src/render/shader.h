#pragma once

#include <vulkan/vulkan.h>

#include <filesystem>

namespace nenet {

[[nodiscard]] VkShaderModule loadShaderModule(VkDevice device, const std::filesystem::path& spvPath);

[[nodiscard]] std::filesystem::path executableDir();

}
