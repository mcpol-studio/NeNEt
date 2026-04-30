#include "chunk_mesh.h"

#include "buffer.h"
#include "vertex.h"
#include "../world/chunk_mesher.h"

#include <spdlog/spdlog.h>

namespace nenet {

ChunkMesh::ChunkMesh(VmaAllocator allocator, const ChunkMeshData& data) {
    uploadFrom(allocator, data);
}

ChunkMesh::~ChunkMesh() = default;

void ChunkMesh::rebuild(VmaAllocator allocator, const ChunkMeshData& data) {
    vertexBuffer_.reset();
    indexBuffer_.reset();
    indexCount_ = 0;
    uploadFrom(allocator, data);
}

void ChunkMesh::uploadFrom(VmaAllocator allocator, const ChunkMeshData& data) {
    if (data.vertices.empty() || data.indices.empty()) {
        spdlog::warn("ChunkMesh: 收到空网格数据");
        return;
    }

    const VkDeviceSize vSize = data.vertices.size() * sizeof(Vertex);
    const VkDeviceSize iSize = data.indices.size() * sizeof(uint32_t);

    vertexBuffer_ = std::make_unique<Buffer>(
        allocator, vSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT);
    vertexBuffer_->upload(data.vertices.data(), vSize);

    indexBuffer_ = std::make_unique<Buffer>(
        allocator, iSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT);
    indexBuffer_->upload(data.indices.data(), iSize);

    indexCount_ = static_cast<uint32_t>(data.indices.size());
    spdlog::info("ChunkMesh GPU 上传完成 vSize={}KB iSize={}KB",
                 vSize / 1024, iSize / 1024);
}

void ChunkMesh::draw(VkCommandBuffer cmd) const {
    if (indexCount_ == 0) return;
    VkBuffer vbufs[] = {vertexBuffer_->handle()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbufs, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer_->handle(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, indexCount_, 1, 0, 0, 0);
}

}
