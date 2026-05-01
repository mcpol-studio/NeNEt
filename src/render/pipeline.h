#pragma once

#include <vulkan/vulkan.h>

namespace nenet {

class CubePipeline {
public:
    CubePipeline(VkDevice device, VkFormat colorFormat, VkFormat depthFormat);
    ~CubePipeline();

    CubePipeline(const CubePipeline&) = delete;
    CubePipeline& operator=(const CubePipeline&) = delete;

    [[nodiscard]] VkPipeline handle() const noexcept { return pipeline_; }
    [[nodiscard]] VkPipelineLayout layout() const noexcept { return layout_; }
    [[nodiscard]] VkDescriptorSetLayout descriptorSetLayout() const noexcept { return descriptorLayout_; }

private:
    VkDevice device_{VK_NULL_HANDLE};
    VkPipeline pipeline_{VK_NULL_HANDLE};
    VkPipelineLayout layout_{VK_NULL_HANDLE};
    VkDescriptorSetLayout descriptorLayout_{VK_NULL_HANDLE};
};

}
