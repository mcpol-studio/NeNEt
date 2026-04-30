#pragma once

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include <cstdint>

namespace nenet {

struct MenuPush {
    glm::vec4 colors[2];
    int32_t hoverIndex;
    int32_t _pad[3];
};
static_assert(sizeof(MenuPush) == 48, "MenuPush 大小不匹配");

class MenuPipeline {
public:
    MenuPipeline(VkDevice device, VkFormat colorFormat, VkFormat depthFormat);
    ~MenuPipeline();

    MenuPipeline(const MenuPipeline&) = delete;
    MenuPipeline& operator=(const MenuPipeline&) = delete;

    [[nodiscard]] VkPipeline handle() const noexcept { return pipeline_; }
    [[nodiscard]] VkPipelineLayout layout() const noexcept { return layout_; }

private:
    VkDevice device_{VK_NULL_HANDLE};
    VkPipeline pipeline_{VK_NULL_HANDLE};
    VkPipelineLayout layout_{VK_NULL_HANDLE};
};

}
