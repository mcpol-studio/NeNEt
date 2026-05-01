#pragma once

#include "block.h"
#include "block_reader.h"
#include "../render/vertex.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace nenet {

class Buffer;

class FallingBlocks {
public:
    explicit FallingBlocks(VmaAllocator allocator);
    ~FallingBlocks();

    FallingBlocks(const FallingBlocks&) = delete;
    FallingBlocks& operator=(const FallingBlocks&) = delete;

    void spawn(const glm::vec3& position, Block block);

    using OnLandFn = std::function<void(const glm::ivec3& pos, Block block)>;
    void update(float dt, const BlockReader& reader, const OnLandFn& onLand);

    void uploadGeometry();
    void draw(VkCommandBuffer cmd) const;

    [[nodiscard]] bool empty() const noexcept { return indexCount_ == 0; }

    struct Entity {
        glm::vec3 position;
        glm::vec3 velocity;
        Block block;
    };
    [[nodiscard]] const std::vector<Entity>& entities() const noexcept { return entities_; }

private:

    static constexpr size_t kMaxEntities = 256;

    VmaAllocator allocator_;
    std::vector<Entity> entities_;
    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;
    std::unique_ptr<Buffer> vertexBuffer_;
    std::unique_ptr<Buffer> indexBuffer_;
    uint32_t indexCount_{0};
};

}
