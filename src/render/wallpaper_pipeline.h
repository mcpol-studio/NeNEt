#pragma once

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include <cstdint>

namespace nenet {

struct WallpaperPush {
    glm::vec2 viewport;
    glm::vec2 imageSize;
    float glassMix;
    float vignette;
    float blurRadius;
    float _pad;
};
static_assert(sizeof(WallpaperPush) == 32, "WallpaperPush 大小不匹配");

class WallpaperPipeline {
public:
    WallpaperPipeline(VkDevice device, VkFormat colorFormat, VkFormat depthFormat);
    ~WallpaperPipeline();

    WallpaperPipeline(const WallpaperPipeline&) = delete;
    WallpaperPipeline& operator=(const WallpaperPipeline&) = delete;

    [[nodiscard]] VkPipeline handle() const noexcept { return pipeline_; }
    [[nodiscard]] VkPipelineLayout layout() const noexcept { return layout_; }
    [[nodiscard]] VkDescriptorSetLayout descriptorSetLayout() const noexcept { return setLayout_; }

private:
    VkDevice device_{VK_NULL_HANDLE};
    VkPipeline pipeline_{VK_NULL_HANDLE};
    VkPipelineLayout layout_{VK_NULL_HANDLE};
    VkDescriptorSetLayout setLayout_{VK_NULL_HANDLE};
};

}
