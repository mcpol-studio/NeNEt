#include "chunk_manager.h"

#include "greedy_mesher.h"
#include "terrain_generator.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace nenet {

namespace {

inline int floorDiv(int a, int b) {
    int q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
    return q;
}
inline int floorMod(int a, int b) {
    int r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}

}

ChunkManager::ChunkManager(VmaAllocator allocator, const TerrainGenerator& gen, int viewDistance)
    : allocator_(allocator), gen_(gen), viewDistance_(viewDistance) {}

ChunkManager::UpdateResult ChunkManager::update(const glm::vec3& cameraPos, int loadBudget, bool animate) {
    const int playerCX = floorDiv(static_cast<int>(std::floor(cameraPos.x)), Chunk::kSizeX);
    const int playerCZ = floorDiv(static_cast<int>(std::floor(cameraPos.z)), Chunk::kSizeZ);

    UpdateResult r{};

    for (int radius = 0; radius <= viewDistance_ && r.loaded < loadBudget; ++radius) {
        for (int dz = -radius; dz <= radius && r.loaded < loadBudget; ++dz) {
            for (int dx = -radius; dx <= radius && r.loaded < loadBudget; ++dx) {
                if (std::max(std::abs(dx), std::abs(dz)) != radius) continue;
                const int cx = playerCX + dx;
                const int cz = playerCZ + dz;
                const Key k = keyOf(cx, cz);
                if (entries_.find(k) != entries_.end()) continue;

                Entry e;
                e.chunk = std::make_unique<Chunk>();
                gen_.generate(*e.chunk, cx, cz);
                const auto md = GreedyMesher::mesh(*e.chunk);
                e.mesh = std::make_unique<ChunkMesh>(allocator_, md);
                e.spawnTime = std::chrono::steady_clock::now();
                e.animate = animate;
                entries_.emplace(k, std::move(e));
                ++r.loaded;
            }
        }
    }

    for (auto it = entries_.begin(); it != entries_.end(); ) {
        const int cx = keyToX(it->first);
        const int cz = keyToZ(it->first);
        const int dx = std::abs(cx - playerCX);
        const int dz = std::abs(cz - playerCZ);
        if (std::max(dx, dz) > viewDistance_ + 1) {
            PendingDelete p;
            p.chunk = std::move(it->second.chunk);
            p.mesh = std::move(it->second.mesh);
            p.framesLeft = 3;
            pending_.push_back(std::move(p));
            it = entries_.erase(it);
            ++r.unloaded;
        } else {
            ++it;
        }
    }

    for (auto it = pending_.begin(); it != pending_.end(); ) {
        if (--it->framesLeft <= 0) {
            it = pending_.erase(it);
        } else {
            ++it;
        }
    }

    if (r.loaded > 0 || r.unloaded > 0) {
        spdlog::info("ChunkManager 加载 {} 卸载 {}（总 {}, 待删 {}）",
                     r.loaded, r.unloaded, entries_.size(), pending_.size());
    }
    return r;
}

Block ChunkManager::worldGet(int worldX, int worldY, int worldZ) const {
    if (worldY < 0 || worldY >= Chunk::kSizeY) return Block::Air;
    const int cx = floorDiv(worldX, Chunk::kSizeX);
    const int cz = floorDiv(worldZ, Chunk::kSizeZ);
    const auto it = entries_.find(keyOf(cx, cz));
    if (it == entries_.end()) return Block::Air;
    const int lx = floorMod(worldX, Chunk::kSizeX);
    const int lz = floorMod(worldZ, Chunk::kSizeZ);
    return it->second.chunk->get(lx, worldY, lz);
}

void ChunkManager::worldSet(int worldX, int worldY, int worldZ, Block b) {
    if (worldY < 0 || worldY >= Chunk::kSizeY) return;
    const int cx = floorDiv(worldX, Chunk::kSizeX);
    const int cz = floorDiv(worldZ, Chunk::kSizeZ);
    const auto it = entries_.find(keyOf(cx, cz));
    if (it == entries_.end()) return;
    const int lx = floorMod(worldX, Chunk::kSizeX);
    const int lz = floorMod(worldZ, Chunk::kSizeZ);
    it->second.chunk->set(lx, worldY, lz, b);
}

void ChunkManager::rebuildMeshAt(int chunkX, int chunkZ) {
    const auto it = entries_.find(keyOf(chunkX, chunkZ));
    if (it == entries_.end()) return;
    const auto md = GreedyMesher::mesh(*it->second.chunk);
    it->second.mesh->rebuild(allocator_, md);
}

}
