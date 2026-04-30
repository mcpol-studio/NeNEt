#pragma once

#include "hud_pipeline.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace nenet {

class Buffer;

class HudRenderer {
public:
    HudRenderer(VmaAllocator allocator, size_t maxQuads = 4096);
    ~HudRenderer();

    HudRenderer(const HudRenderer&) = delete;
    HudRenderer& operator=(const HudRenderer&) = delete;

    void clear();
    void addText(const std::string& s, float ndcX, float ndcY,
                 float pixelSize, const glm::vec3& color);
    void addRect(float x0, float y0, float x1, float y1, const glm::vec3& color);
    void upload();
    void draw(VkCommandBuffer cmd) const;

    [[nodiscard]] bool empty() const noexcept { return indexCount_ == 0; }

private:
    VmaAllocator allocator_;
    size_t maxQuads_;
    std::unique_ptr<Buffer> vertexBuffer_;
    std::unique_ptr<Buffer> indexBuffer_;
    std::vector<HudVertex> vertices_;
    std::vector<uint32_t> indices_;
    uint32_t indexCount_{0};
};

}
