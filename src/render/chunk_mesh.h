#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <cstdint>
#include <memory>

namespace nenet {

struct ChunkMeshData;
class Buffer;

class ChunkMesh {
public:
    ChunkMesh(VmaAllocator allocator, const ChunkMeshData& data);
    ~ChunkMesh();

    ChunkMesh(const ChunkMesh&) = delete;
    ChunkMesh& operator=(const ChunkMesh&) = delete;

    void rebuild(VmaAllocator allocator, const ChunkMeshData& data);
    void draw(VkCommandBuffer cmd) const;

    [[nodiscard]] uint32_t indexCount() const noexcept { return indexCount_; }
    [[nodiscard]] bool empty() const noexcept { return indexCount_ == 0; }

private:
    void uploadFrom(VmaAllocator allocator, const ChunkMeshData& data);

    std::unique_ptr<Buffer> vertexBuffer_;
    std::unique_ptr<Buffer> indexBuffer_;
    uint32_t indexCount_{0};
};

}
