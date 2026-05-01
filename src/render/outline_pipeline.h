#pragma once

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

namespace nenet {

struct OutlinePush {
    glm::mat4 mvp;
    glm::vec4 color;
};
static_assert(sizeof(OutlinePush) == 80, "OutlinePush 大小不匹配");

class OutlinePipeline {
public:
    OutlinePipeline(VkDevice device, VkFormat colorFormat, VkFormat depthFormat);
    ~OutlinePipeline();

    OutlinePipeline(const OutlinePipeline&) = delete;
    OutlinePipeline& operator=(const OutlinePipeline&) = delete;

    [[nodiscard]] VkPipeline handle() const noexcept { return pipeline_; }
    [[nodiscard]] VkPipelineLayout layout() const noexcept { return layout_; }

private:
    VkDevice device_{VK_NULL_HANDLE};
    VkPipeline pipeline_{VK_NULL_HANDLE};
    VkPipelineLayout layout_{VK_NULL_HANDLE};
};

}
