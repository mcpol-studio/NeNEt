#pragma once

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <array>

namespace nenet {

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
    glm::vec2 uv{0.0f, 0.0f};
    glm::vec2 atlasOrigin{0.0f, 0.0f};

    static VkVertexInputBindingDescription binding() {
        VkVertexInputBindingDescription b{};
        b.binding = 0;
        b.stride = sizeof(Vertex);
        b.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return b;
    }

    static std::array<VkVertexInputAttributeDescription, 4> attributes() {
        std::array<VkVertexInputAttributeDescription, 4> a{};
        a[0].location = 0;
        a[0].binding = 0;
        a[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        a[0].offset = offsetof(Vertex, position);
        a[1].location = 1;
        a[1].binding = 0;
        a[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        a[1].offset = offsetof(Vertex, color);
        a[2].location = 2;
        a[2].binding = 0;
        a[2].format = VK_FORMAT_R32G32_SFLOAT;
        a[2].offset = offsetof(Vertex, uv);
        a[3].location = 3;
        a[3].binding = 0;
        a[3].format = VK_FORMAT_R32G32_SFLOAT;
        a[3].offset = offsetof(Vertex, atlasOrigin);
        return a;
    }
};

struct PushConstants {
    glm::mat4 mvp;
    glm::vec4 tint{0.0f, 0.0f, 0.0f, 0.0f};
};

}
