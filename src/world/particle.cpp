#include "particle.h"

#include "../render/buffer.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <random>

namespace nenet {

namespace {

constexpr float kGravity = -18.0f;
constexpr int kParticlesPerBreak = 14;

constexpr glm::vec3 kCubeCorners[8] = {
    {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f},
    {-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f},
};

constexpr uint32_t kCubeIndicesData[36] = {
    0,1,2, 2,3,0,
    4,6,5, 4,7,6,
    4,5,1, 1,0,4,
    3,2,6, 6,7,3,
    1,5,6, 6,2,1,
    4,0,3, 3,7,4,
};

float frand01() {
    static std::mt19937 rng(0xCAFEBABE);
    static std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rng);
}

float frand11() {
    return frand01() * 2.0f - 1.0f;
}

}

ParticleSystem::ParticleSystem(VmaAllocator allocator) : allocator_(allocator) {
    vertexBuffer_ = std::make_unique<Buffer>(
        allocator_, kMaxParticles * kVerticesPerParticle * sizeof(Vertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT);
    indexBuffer_ = std::make_unique<Buffer>(
        allocator_, kMaxParticles * kIndicesPerParticle * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT);

    particles_.reserve(kMaxParticles);
    vertices_.reserve(kMaxParticles * kVerticesPerParticle);
    indices_.reserve(kMaxParticles * kIndicesPerParticle);
    spdlog::info("ParticleSystem 已初始化 maxParticles={}", kMaxParticles);
}

ParticleSystem::~ParticleSystem() = default;

void ParticleSystem::spawnBlockBreak(const glm::ivec3& blockPos, Block block) {
    const glm::vec3 base(blockPos.x + 0.5f, blockPos.y + 0.5f, blockPos.z + 0.5f);
    const glm::vec3 baseColor = blockColor(block);

    for (int i = 0; i < kParticlesPerBreak; ++i) {
        if (particles_.size() >= kMaxParticles) break;

        Particle p;
        p.position = base + glm::vec3(frand11() * 0.30f, frand11() * 0.30f, frand11() * 0.30f);
        p.velocity = glm::vec3(frand11() * 2.5f, frand01() * 4.0f + 1.0f, frand11() * 2.5f);
        p.color = baseColor * (0.85f + frand01() * 0.30f);
        p.lifetime = 0.5f + frand01() * 0.4f;
        p.size = 0.10f + frand01() * 0.06f;
        particles_.push_back(p);
    }
}

void ParticleSystem::update(float dt) {
    for (auto& p : particles_) {
        p.velocity.y += kGravity * dt;
        p.position += p.velocity * dt;
        p.lifetime -= dt;
    }
    particles_.erase(
        std::remove_if(particles_.begin(), particles_.end(),
                       [](const Particle& p) { return p.lifetime <= 0.0f; }),
        particles_.end());
}

void ParticleSystem::uploadGeometry() {
    vertices_.clear();
    indices_.clear();

    for (const auto& p : particles_) {
        const float fade = std::clamp(p.lifetime * 2.0f, 0.0f, 1.0f);
        const glm::vec3 c = p.color * fade;
        const uint32_t base = static_cast<uint32_t>(vertices_.size());
        for (const auto& corner : kCubeCorners) {
            Vertex v;
            v.position = p.position + corner * p.size;
            v.color = c;
            vertices_.push_back(v);
        }
        for (uint32_t idx : kCubeIndicesData) {
            indices_.push_back(base + idx);
        }
    }

    if (vertices_.empty() || indices_.empty()) {
        indexCount_ = 0;
        return;
    }

    vertexBuffer_->upload(vertices_.data(), vertices_.size() * sizeof(Vertex));
    indexBuffer_->upload(indices_.data(), indices_.size() * sizeof(uint32_t));
    indexCount_ = static_cast<uint32_t>(indices_.size());
}

void ParticleSystem::draw(VkCommandBuffer cmd) const {
    if (indexCount_ == 0) return;
    VkBuffer vbufs[] = {vertexBuffer_->handle()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbufs, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer_->handle(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, indexCount_, 1, 0, 0, 0);
}

}
