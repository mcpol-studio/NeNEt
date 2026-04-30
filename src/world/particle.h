#pragma once

#include "block.h"
#include "../render/vertex.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace nenet {

class Buffer;

class ParticleSystem {
public:
    explicit ParticleSystem(VmaAllocator allocator);
    ~ParticleSystem();

    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;

    void spawnBlockBreak(const glm::ivec3& blockPos, Block block);
    void update(float dt);
    void uploadGeometry();
    void draw(VkCommandBuffer cmd) const;

    [[nodiscard]] bool empty() const noexcept { return indexCount_ == 0; }
    [[nodiscard]] size_t count() const noexcept { return particles_.size(); }

private:
    struct Particle {
        glm::vec3 position;
        glm::vec3 velocity;
        glm::vec3 color;
        float lifetime;
        float size;
    };

    static constexpr size_t kMaxParticles = 256;
    static constexpr size_t kVerticesPerParticle = 8;
    static constexpr size_t kIndicesPerParticle = 36;

    VmaAllocator allocator_;
    std::vector<Particle> particles_;
    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;
    std::unique_ptr<Buffer> vertexBuffer_;
    std::unique_ptr<Buffer> indexBuffer_;
    uint32_t indexCount_{0};
};

}
