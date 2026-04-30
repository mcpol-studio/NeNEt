#pragma once

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include <cstdint>

namespace nenet {

struct HotbarPush {
    glm::vec4 colors[6];
    int32_t selectedSlot;
    int32_t _pad[3];
};
static_assert(sizeof(HotbarPush) == 112, "HotbarPush 大小不匹配 GLSL push_constant 布局");

class HotbarPipeline {
public:
    HotbarPipeline(VkDevice device, VkFormat colorFormat, VkFormat depthFormat);
    ~HotbarPipeline();

    HotbarPipeline(const HotbarPipeline&) = delete;
    HotbarPipeline& operator=(const HotbarPipeline&) = delete;

    [[nodiscard]] VkPipeline handle() const noexcept { return pipeline_; }
    [[nodiscard]] VkPipelineLayout layout() const noexcept { return layout_; }

private:
    VkDevice device_{VK_NULL_HANDLE};
    VkPipeline pipeline_{VK_NULL_HANDLE};
    VkPipelineLayout layout_{VK_NULL_HANDLE};
};

}
