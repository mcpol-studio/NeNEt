#include "terrain_generator.h"

#include "block.h"
#include "chunk.h"

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/noise.hpp>
#include <glm/vec3.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>

namespace nenet {

TerrainGenerator::TerrainGenerator(uint64_t seed) : seed_(seed) {}

float TerrainGenerator::sampleHeight01(int worldX, int worldZ) const {
    const float ox = static_cast<float>(seed_ & 0xFFFFu) * 0.131f;
    const float oz = static_cast<float>((seed_ >> 16) & 0xFFFFu) * 0.197f;

    float total = 0.0f;
    float maxValue = 0.0f;
    float amplitude = 1.0f;
    float frequency = frequency_;

    for (int i = 0; i < octaves_; ++i) {
        const float n = glm::perlin(glm::vec2(
            worldX * frequency + ox,
            worldZ * frequency + oz));
        total += n * amplitude;
        maxValue += amplitude;
        amplitude *= persistence_;
        frequency *= lacunarity_;
    }

    return total / maxValue;
}

bool TerrainGenerator::isCave(int worldX, int y, int worldZ) const {
    const float ox = static_cast<float>(seed_ & 0xFFFFu) * 0.071f;
    const float oy = static_cast<float>((seed_ >> 16) & 0xFFFFu) * 0.137f;
    const float oz = static_cast<float>((seed_ >> 32) & 0xFFFFu) * 0.211f;

    const float n = glm::perlin(glm::vec3(
        worldX * caveFrequency_ + ox,
        y * caveFrequency_ * caveYStretch_ + oy,
        worldZ * caveFrequency_ + oz));
    return n > caveThreshold_;
}

void TerrainGenerator::generate(Chunk& chunk, int chunkX, int chunkZ) const {
    const auto t0 = std::chrono::steady_clock::now();

    int hMin = Chunk::kSizeY;
    int hMax = 0;

    for (int z = 0; z < Chunk::kSizeZ; ++z) {
        for (int x = 0; x < Chunk::kSizeX; ++x) {
            const int worldX = chunkX * Chunk::kSizeX + x;
            const int worldZ = chunkZ * Chunk::kSizeZ + z;

            const float n = sampleHeight01(worldX, worldZ);
            const int h = std::clamp(
                baseHeight_ + static_cast<int>(n * amplitude_),
                1, Chunk::kSizeY - 1);

            hMin = std::min(hMin, h);
            hMax = std::max(hMax, h);

            const bool underwater = h < seaLevel_;
            const bool isBeach = (h >= seaLevel_ - 2 && h <= seaLevel_ + 2);

            Block topMat;
            Block subMat;
            if (isBeach) {
                topMat = Block::Sand;
                subMat = Block::Sand;
            } else if (underwater) {
                topMat = Block::Dirt;
                subMat = Block::Dirt;
            } else {
                topMat = Block::Grass;
                subMat = Block::Dirt;
            }

            for (int y = 0; y <= h; ++y) {
                Block b;
                if (y == 0) {
                    b = Block::Stone;
                } else if (y == h) {
                    b = topMat;
                } else if (y >= h - dirtDepth_) {
                    b = subMat;
                } else {
                    b = Block::Stone;
                }

                if (y > 0 && y < h - surfaceProtect_ && isCave(worldX, y, worldZ)) {
                    b = Block::Air;
                }

                chunk.set(x, y, z, b);
            }

            for (int y = h + 1; y <= seaLevel_; ++y) {
                chunk.set(x, y, z, Block::Water);
            }
        }
    }

    placeTrees(chunk, chunkX, chunkZ);

    const auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    spdlog::info("Chunk ({},{}) 已生成 hRange=[{},{}] base={} amp={} 耗时={:.2f}ms",
                 chunkX, chunkZ, hMin, hMax, baseHeight_, amplitude_, ms);
}

namespace {

uint64_t hashCoord(int x, int z, uint64_t seed, uint64_t salt) {
    uint64_t h = seed
                 ^ (static_cast<uint64_t>(x) * 341873128712ull)
                 ^ (static_cast<uint64_t>(z) * 132897987541ull)
                 ^ (salt * 6364136223846793005ull);
    h ^= h >> 30;
    h *= 0xbf58476d1ce4e5b9ull;
    h ^= h >> 27;
    h *= 0x94d049bb133111ebull;
    h ^= h >> 31;
    return h;
}

}

void TerrainGenerator::placeTrees(Chunk& chunk, int chunkX, int chunkZ) const {
    int treeCount = 0;
    const int margin = 2;

    for (int z = margin; z < Chunk::kSizeZ - margin; ++z) {
        for (int x = margin; x < Chunk::kSizeX - margin; ++x) {
            int h = -1;
            for (int y = Chunk::kSizeY - 1; y >= 0; --y) {
                if (chunk.get(x, y, z) == Block::Grass) { h = y; break; }
                if (chunk.get(x, y, z) != Block::Air) break;
            }
            if (h < 0) continue;
            if (h + 7 >= Chunk::kSizeY) continue;

            const int worldX = chunkX * Chunk::kSizeX + x;
            const int worldZ = chunkZ * Chunk::kSizeZ + z;
            const uint64_t hash = hashCoord(worldX, worldZ, seed_, 0xA1B2u);
            if ((hash & 0x3F) != 0) continue;

            for (int dy = 1; dy <= 5; ++dy) {
                chunk.set(x, h + dy, z, Block::Wood);
            }

            for (int dy = 3; dy <= 4; ++dy) {
                for (int dz = -2; dz <= 2; ++dz) {
                    for (int dx = -2; dx <= 2; ++dx) {
                        if (std::abs(dx) == 2 && std::abs(dz) == 2) continue;
                        if (dx == 0 && dz == 0) continue;
                        if (chunk.get(x + dx, h + dy, z + dz) == Block::Air) {
                            chunk.set(x + dx, h + dy, z + dz, Block::Leaves);
                        }
                    }
                }
            }

            for (int dy = 5; dy <= 6; ++dy) {
                for (int dz = -1; dz <= 1; ++dz) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dy == 5 && dx == 0 && dz == 0) continue;
                        if (std::abs(dx) == 1 && std::abs(dz) == 1 && dy == 6) continue;
                        if (chunk.get(x + dx, h + dy, z + dz) == Block::Air) {
                            chunk.set(x + dx, h + dy, z + dz, Block::Leaves);
                        }
                    }
                }
            }

            ++treeCount;
        }
    }

    if (treeCount > 0) {
        spdlog::info("Chunk ({},{}) 长树 {} 棵", chunkX, chunkZ, treeCount);
    }
}

}
