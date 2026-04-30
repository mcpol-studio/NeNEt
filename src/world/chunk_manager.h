#pragma once

#include "block.h"
#include "chunk.h"
#include "../render/chunk_mesh.h"

#include <vk_mem_alloc.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace nenet {

class TerrainGenerator;

class ChunkManager {
public:
    using Key = int64_t;

    struct Entry {
        std::unique_ptr<Chunk> chunk;
        std::unique_ptr<ChunkMesh> mesh;
        std::chrono::steady_clock::time_point spawnTime{};
        bool animate{true};
    };

    struct UpdateResult {
        int loaded{0};
        int unloaded{0};
        [[nodiscard]] bool needGpuWait() const noexcept { return false; }
    };

    ChunkManager(VmaAllocator allocator, const TerrainGenerator& gen, int viewDistance);
    ~ChunkManager() = default;

    ChunkManager(const ChunkManager&) = delete;
    ChunkManager& operator=(const ChunkManager&) = delete;

    UpdateResult update(const glm::vec3& cameraPos, int loadBudget, bool animate = true);

    [[nodiscard]] Block worldGet(int worldX, int worldY, int worldZ) const;
    void worldSet(int worldX, int worldY, int worldZ, Block b);
    void rebuildMeshAt(int chunkX, int chunkZ);

    [[nodiscard]] const std::unordered_map<Key, Entry>& entries() const noexcept { return entries_; }
    [[nodiscard]] int viewDistance() const noexcept { return viewDistance_; }

    [[nodiscard]] static constexpr Key keyOf(int x, int z) noexcept {
        return (static_cast<int64_t>(x) << 32) | static_cast<uint32_t>(z);
    }
    [[nodiscard]] static constexpr int keyToX(Key k) noexcept {
        return static_cast<int>(k >> 32);
    }
    [[nodiscard]] static constexpr int keyToZ(Key k) noexcept {
        return static_cast<int>(static_cast<int32_t>(static_cast<uint32_t>(k)));
    }

private:
    struct PendingDelete {
        std::unique_ptr<Chunk> chunk;
        std::unique_ptr<ChunkMesh> mesh;
        int framesLeft{0};
    };

    VmaAllocator allocator_;
    const TerrainGenerator& gen_;
    int viewDistance_;
    std::unordered_map<Key, Entry> entries_;
    std::vector<PendingDelete> pending_;
};

}
