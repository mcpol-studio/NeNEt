#include "falling_blocks.h"

#include "../render/buffer.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>

namespace nenet {

namespace {

constexpr float kGravity = -28.0f;
constexpr float kMaxFall = -54.0f;

struct FaceDef {
    glm::vec3 normal;
    glm::vec3 corners[4];
};

constexpr FaceDef kFaces[6] = {
    {{ 1, 0, 0}, {{1,0,0}, {1,1,0}, {1,1,1}, {1,0,1}}},
    {{-1, 0, 0}, {{0,0,1}, {0,1,1}, {0,1,0}, {0,0,0}}},
    {{ 0, 1, 0}, {{0,1,0}, {0,1,1}, {1,1,1}, {1,1,0}}},
    {{ 0,-1, 0}, {{0,0,0}, {1,0,0}, {1,0,1}, {0,0,1}}},
    {{ 0, 0, 1}, {{0,0,1}, {1,0,1}, {1,1,1}, {0,1,1}}},
    {{ 0, 0,-1}, {{1,0,0}, {0,0,0}, {0,1,0}, {1,1,0}}},
};

Face faceEnum(int idx) {
    switch (idx) {
        case 0: return Face::PosX;
        case 1: return Face::NegX;
        case 2: return Face::PosY;
        case 3: return Face::NegY;
        case 4: return Face::PosZ;
        default: return Face::NegZ;
    }
}

glm::vec3 shadeForNormal(const glm::vec3& n) {
    if (n.y > 0.5f) return glm::vec3(1.00f);
    if (n.y < -0.5f) return glm::vec3(0.55f);
    if (std::abs(n.x) > 0.5f) return glm::vec3(0.85f);
    return glm::vec3(0.70f);
}

}

FallingBlocks::FallingBlocks(VmaAllocator allocator) : allocator_(allocator) {
    constexpr size_t kVertsPerEntity = 24;
    constexpr size_t kIndicesPerEntity = 36;
    vertexBuffer_ = std::make_unique<Buffer>(
        allocator_, kMaxEntities * kVertsPerEntity * sizeof(Vertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT);
    indexBuffer_ = std::make_unique<Buffer>(
        allocator_, kMaxEntities * kIndicesPerEntity * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT);
    entities_.reserve(kMaxEntities);
    vertices_.reserve(kMaxEntities * kVertsPerEntity);
    indices_.reserve(kMaxEntities * kIndicesPerEntity);
}

FallingBlocks::~FallingBlocks() = default;

void FallingBlocks::spawn(const glm::vec3& position, Block block) {
    if (entities_.size() >= kMaxEntities) return;
    entities_.push_back({position, glm::vec3(0.0f), block});
}

void FallingBlocks::update(float dt, const BlockReader& reader, const OnLandFn& onLand) {
    auto isSupport = [](Block b) {
        return b != Block::Air && b != Block::Water &&
               b != Block::Lava && !isCrossPlant(b);
    };

    for (auto it = entities_.begin(); it != entities_.end(); ) {
        it->velocity.y += kGravity * dt;
        if (it->velocity.y < kMaxFall) it->velocity.y = kMaxFall;

        const int bx = static_cast<int>(std::floor(it->position.x));
        const int bz = static_cast<int>(std::floor(it->position.z));
        const int currentY = static_cast<int>(std::floor(it->position.y));
        const float newY = it->position.y + it->velocity.y * dt;
        const int newBottom = static_cast<int>(std::floor(newY));

        int landY = currentY;
        bool landed = false;
        const int scanLow = std::min(currentY - 1, newBottom);
        for (int y = currentY - 1; y >= scanLow && y >= 0; --y) {
            if (isSupport(reader(bx, y, bz))) {
                landY = y + 1;
                landed = true;
                break;
            }
        }

        const float topOfSupport = static_cast<float>(landY);

        if (landed && newY <= topOfSupport) {
            if (std::abs(it->position.y - topOfSupport) > 1e-4f) {
                it->position.y = topOfSupport;
                it->velocity.y = 0.0f;
                ++it;
            } else {
                onLand({bx, landY, bz}, it->block);
                it = entities_.erase(it);
            }
        } else {
            it->position.y = newY;
            ++it;
        }
    }
}

void FallingBlocks::uploadGeometry() {
    vertices_.clear();
    indices_.clear();

    for (const auto& e : entities_) {
        for (int f = 0; f < 6; ++f) {
            const auto& face = kFaces[f];
            const glm::ivec2 slot = atlasSlot(e.block, faceEnum(f));
            const glm::vec2 atlasOrigin = glm::vec2(slot) * (1.0f / 64.0f);
            const glm::vec3 color = shadeForNormal(face.normal);

            const glm::vec2 uv00(0.0f, 1.0f);
            const glm::vec2 uv10(1.0f, 1.0f);
            const glm::vec2 uv11(1.0f, 0.0f);
            const glm::vec2 uv01(0.0f, 0.0f);

            const uint32_t base = static_cast<uint32_t>(vertices_.size());
            for (int i = 0; i < 4; ++i) {
                Vertex v;
                v.position = e.position + face.corners[i];
                v.color = color;
                if (i == 0)      v.uv = uv00;
                else if (i == 1) v.uv = uv10;
                else if (i == 2) v.uv = uv11;
                else             v.uv = uv01;
                v.atlasOrigin = atlasOrigin;
                vertices_.push_back(v);
            }
            indices_.push_back(base + 0);
            indices_.push_back(base + 1);
            indices_.push_back(base + 2);
            indices_.push_back(base + 0);
            indices_.push_back(base + 2);
            indices_.push_back(base + 3);
        }
    }

    if (vertices_.empty()) {
        indexCount_ = 0;
        return;
    }
    vertexBuffer_->upload(vertices_.data(), vertices_.size() * sizeof(Vertex));
    indexBuffer_->upload(indices_.data(), indices_.size() * sizeof(uint32_t));
    indexCount_ = static_cast<uint32_t>(indices_.size());
}

void FallingBlocks::draw(VkCommandBuffer cmd) const {
    if (indexCount_ == 0) return;
    VkBuffer vbufs[] = {vertexBuffer_->handle()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbufs, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer_->handle(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, indexCount_, 1, 0, 0, 0);
}

}
