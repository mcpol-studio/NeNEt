#pragma once

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include <array>
#include <cstdint>

namespace nenet {

struct HudVertex {
    glm::vec2 pos;
    glm::vec3 color;

    static VkVertexInputBindingDescription binding() {
        VkVertexInputBindingDescription b{};
        b.binding = 0;
        b.stride = sizeof(HudVertex);
        b.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return b;
    }

    static std::array<VkVertexInputAttributeDescription, 2> attributes() {
        std::array<VkVertexInputAttributeDescription, 2> a{};
        a[0].location = 0;
        a[0].binding = 0;
        a[0].format = VK_FORMAT_R32G32_SFLOAT;
        a[0].offset = offsetof(HudVertex, pos);
        a[1].location = 1;
        a[1].binding = 0;
        a[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        a[1].offset = offsetof(HudVertex, color);
        return a;
    }
};

class HudPipeline {
public:
    HudPipeline(VkDevice device, VkFormat colorFormat, VkFormat depthFormat);
    ~HudPipeline();

    HudPipeline(const HudPipeline&) = delete;
    HudPipeline& operator=(const HudPipeline&) = delete;

    [[nodiscard]] VkPipeline handle() const noexcept { return pipeline_; }
    [[nodiscard]] VkPipelineLayout layout() const noexcept { return layout_; }

private:
    VkDevice device_{VK_NULL_HANDLE};
    VkPipeline pipeline_{VK_NULL_HANDLE};
    VkPipelineLayout layout_{VK_NULL_HANDLE};
};

}
