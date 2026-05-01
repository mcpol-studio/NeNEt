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
    : allocator_(allocator), gen_(gen), viewDistance_(viewDistance) {
    unsigned hw = std::thread::hardware_concurrency();
    int n = static_cast<int>(hw > 0 ? hw / 2 : 2);
    if (n < 2) n = 2;
    if (n > 6) n = 6;
    workers_.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        workers_.emplace_back(&ChunkManager::workerLoop, this);
    }
    spdlog::info("ChunkManager 启动 {} 个 worker 线程", n);
}

ChunkManager::~ChunkManager() {
    {
        std::lock_guard<std::mutex> lk(queueMu_);
        stop_.store(true);
    }
    queueCv_.notify_all();
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
}

void ChunkManager::workerLoop() {
    while (true) {
        JobReq job{};
        {
            std::unique_lock<std::mutex> lk(queueMu_);
            queueCv_.wait(lk, [&] { return stop_.load() || !pendingJobs_.empty(); });
            if (stop_.load()) return;

            auto best = pendingJobs_.begin();
            for (auto it = pendingJobs_.begin(); it != pendingJobs_.end(); ++it) {
                if (it->priority < best->priority) best = it;
            }
            job = *best;
            pendingJobs_.erase(best);
        }

        auto chunk = std::make_unique<Chunk>();
        if (!emptyMode_.load()) {
            gen_.generate(*chunk, job.cx, job.cz);
        }
        ChunkMeshData md = GreedyMesher::mesh(*chunk);

        {
            std::lock_guard<std::mutex> lk(queueMu_);
            JobDone d;
            d.cx = job.cx;
            d.cz = job.cz;
            d.chunk = std::move(chunk);
            d.meshData = std::move(md);
            doneJobs_.push_back(std::move(d));
        }
    }
}

void ChunkManager::enqueueJob(int cx, int cz, int priority) {
    const Key k = keyOf(cx, cz);
    if (entries_.find(k) != entries_.end()) return;
    if (inflight_.find(k) != inflight_.end()) return;
    inflight_.insert(k);
    {
        std::lock_guard<std::mutex> lk(queueMu_);
        pendingJobs_.push_back(JobReq{cx, cz, priority});
    }
    queueCv_.notify_one();
}

ChunkManager::UpdateResult ChunkManager::update(const glm::vec3& cameraPos, int loadBudget, bool animate) {
    const int playerCX = floorDiv(static_cast<int>(std::floor(cameraPos.x)), Chunk::kSizeX);
    const int playerCZ = floorDiv(static_cast<int>(std::floor(cameraPos.z)), Chunk::kSizeZ);

    UpdateResult r{};

    std::vector<JobDone> drained;
    {
        std::lock_guard<std::mutex> lk(queueMu_);
        if (!doneJobs_.empty()) {
            const size_t take = std::min(doneJobs_.size(), static_cast<size_t>(std::max(loadBudget, 1)));
            drained.reserve(take);
            for (size_t i = 0; i < take; ++i) {
                drained.push_back(std::move(doneJobs_[i]));
            }
            doneJobs_.erase(doneJobs_.begin(), doneJobs_.begin() + take);
        }
    }
    for (auto& d : drained) {
        const Key k = keyOf(d.cx, d.cz);
        inflight_.erase(k);
        if (entries_.find(k) != entries_.end()) continue;
        Entry e;
        e.chunk = std::move(d.chunk);
        e.mesh = std::make_unique<ChunkMesh>(allocator_, d.meshData);
        e.spawnTime = std::chrono::steady_clock::now();
        e.animate = animate;
        entries_.emplace(k, std::move(e));
        ++r.loaded;
    }

    constexpr int kMaxInflight = 8;
    if (inflight_.size() < kMaxInflight) {
        for (int radius = 0; radius <= viewDistance_; ++radius) {
            if (inflight_.size() >= kMaxInflight) break;
            for (int dz = -radius; dz <= radius; ++dz) {
                if (inflight_.size() >= kMaxInflight) break;
                for (int dx = -radius; dx <= radius; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dz)) != radius) continue;
                    const int cx = playerCX + dx;
                    const int cz = playerCZ + dz;
                    enqueueJob(cx, cz, dx * dx + dz * dz);
                    if (inflight_.size() >= kMaxInflight) break;
                }
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
        spdlog::info("ChunkManager 加载 {} 卸载 {}（总 {}, 待删 {}, inflight {}）",
                     r.loaded, r.unloaded, entries_.size(), pending_.size(), inflight_.size());
    }
    (void)animate;
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

void ChunkManager::ensureChunk(int chunkX, int chunkZ) {
    const Key k = keyOf(chunkX, chunkZ);
    if (entries_.find(k) != entries_.end()) return;
    Entry e;
    e.chunk = std::make_unique<Chunk>();
    const auto md = GreedyMesher::mesh(*e.chunk);
    e.mesh = std::make_unique<ChunkMesh>(allocator_, md);
    e.spawnTime = std::chrono::steady_clock::now();
    e.animate = false;
    entries_.emplace(k, std::move(e));
}

void ChunkManager::setEmptyMode(bool empty) {
    if (emptyMode_.load() == empty) return;
    emptyMode_.store(empty);
    if (!empty) return;
    for (auto& [key, entry] : entries_) {
        if (!entry.chunk) continue;
        entry.chunk->clear();
        if (entry.mesh) {
            const auto md = GreedyMesher::mesh(*entry.chunk);
            entry.mesh->rebuild(allocator_, md);
        }
    }
    spdlog::info("ChunkManager 进入空地形模式，已清空 {} 个 chunks", entries_.size());
}

void ChunkManager::rebuildMeshAt(int chunkX, int chunkZ) {
    const auto it = entries_.find(keyOf(chunkX, chunkZ));
    if (it == entries_.end()) return;
    const auto md = GreedyMesher::mesh(*it->second.chunk);
    it->second.mesh->rebuild(allocator_, md);
}

}
