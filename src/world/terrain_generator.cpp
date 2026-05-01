#include "terrain_generator.h"

#include "biome.h"
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
    int colH[Chunk::kSizeX * Chunk::kSizeZ]{};

    for (int z = 0; z < Chunk::kSizeZ; ++z) {
        for (int x = 0; x < Chunk::kSizeX; ++x) {
            const int worldX = chunkX * Chunk::kSizeX + x;
            const int worldZ = chunkZ * Chunk::kSizeZ + z;

            const float biomeT = biomeRawNoise(worldX, worldZ, seed_);
            const Biome biome = biomeAt(worldX, worldZ, seed_);

            float ampMul = 1.0f;
            if (biomeT > 0.45f) {
                const float k = std::clamp((biomeT - 0.45f) / 0.40f, 0.0f, 1.0f);
                const float smooth = k * k * (3.0f - 2.0f * k);
                ampMul = 1.0f + smooth * (mountainAmpMul_ - 1.0f);
            }

            const float n = sampleHeight01(worldX, worldZ);
            const int h = std::clamp(
                baseHeight_ + static_cast<int>(n * amplitude_ * ampMul),
                1, Chunk::kSizeY - 1);

            hMin = std::min(hMin, h);
            hMax = std::max(hMax, h);
            colH[z * Chunk::kSizeX + x] = h;

            const bool underwater = h < seaLevel_;
            const bool isBeach = (h >= seaLevel_ - 2 && h <= seaLevel_ + 2);

            Block topMat;
            Block subMat;
            if (biome == Biome::Desert) {
                topMat = Block::Sand;
                subMat = Block::Sand;
            } else if (isBeach) {
                topMat = Block::Sand;
                subMat = Block::Sand;
            } else if (underwater) {
                topMat = Block::Dirt;
                subMat = Block::Dirt;
            } else if (biome == Biome::Mountain && h >= snowLine_) {
                topMat = Block::Snow;
                subMat = Block::Stone;
            } else {
                topMat = biomeTopMat(biome);
                subMat = biomeSubMat(biome);
            }

            for (int y = 0; y <= h; ++y) {
                Block b;
                if (y == 0) {
                    b = Block::Stone;
                } else if (y == h) {
                    b = topMat;
                } else if (y >= h - dirtDepth_) {
                    if (biome == Biome::Mesa) {
                        const int rel = h - y;
                        if (rel == 1)      b = Block::MesaOrange;
                        else if (rel == 2) b = Block::MesaYellow;
                        else               b = Block::MesaWhite;
                    } else {
                        b = subMat;
                    }
                } else {
                    b = Block::Stone;
                }

                if (y > 0 && y < h - surfaceProtect_ && isCave(worldX, y, worldZ)) {
                    if (y < lavaLevel_) {
                        b = Block::Lava;
                    } else {
                        b = Block::Air;
                    }
                }

                chunk.set(x, y, z, b);
            }

            for (int y = h + 1; y <= seaLevel_; ++y) {
                chunk.set(x, y, z, Block::Water);
            }
        }
    }

    const auto tCore = std::chrono::steady_clock::now();
    placeTrees(chunk, chunkX, chunkZ, colH);
    placeDecorations(chunk, chunkX, chunkZ, colH);
    placeSugarCane(chunk, chunkX, chunkZ, colH);
    (void)&TerrainGenerator::placeWaterDecorations;

    const auto t1 = std::chrono::steady_clock::now();
    const double msCore = std::chrono::duration<double, std::milli>(tCore - t0).count();
    const double msPlace = std::chrono::duration<double, std::milli>(t1 - tCore).count();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    spdlog::info("Chunk ({},{}) hRange=[{},{}] core={:.2f}ms place={:.2f}ms total={:.2f}ms",
                 chunkX, chunkZ, hMin, hMax, msCore, msPlace, ms);
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

float forestNoise(int worldX, int worldZ, uint64_t seed) {
    const uint64_t mixed = seed ^ 0xCAFEFEED12345678ull;
    const float ox = static_cast<float>(mixed & 0xFFFFu) * 0.029f;
    const float oz = static_cast<float>((mixed >> 16) & 0xFFFFu) * 0.061f;
    return glm::perlin(glm::vec2(
        worldX * 0.008f + ox,
        worldZ * 0.008f + oz));
}

}

void TerrainGenerator::placeTrees(Chunk& chunk, int chunkX, int chunkZ, const int* colH) const {
    int treeCount = 0;
    const int margin = 2;

    for (int z = margin; z < Chunk::kSizeZ - margin; ++z) {
        for (int x = margin; x < Chunk::kSizeX - margin; ++x) {
            const int h = colH[z * Chunk::kSizeX + x];
            if (h <= 0) continue;
            if (h + 7 >= Chunk::kSizeY) continue;
            if (chunk.get(x, h, z) != Block::Grass) continue;
            if (chunk.get(x, h + 1, z) != Block::Air) continue;

            const int worldX = chunkX * Chunk::kSizeX + x;
            const int worldZ = chunkZ * Chunk::kSizeZ + z;
            if (!biomeAllowsTrees(biomeAt(worldX, worldZ, seed_))) continue;
            const float forestT = forestNoise(worldX, worldZ, seed_);
            const bool isForest = forestT > 0.30f;
            const uint64_t hashMask = isForest ? 0x0Fu : 0x3Fu;
            const uint64_t hash = hashCoord(worldX, worldZ, seed_, 0xA1B2u);
            if ((hash & hashMask) != 0) continue;

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

void TerrainGenerator::placeDecorations(Chunk& chunk, int chunkX, int chunkZ, const int* colH) const {
    int flowers = 0;
    int grasses = 0;
    for (int z = 0; z < Chunk::kSizeZ; ++z) {
        for (int x = 0; x < Chunk::kSizeX; ++x) {
            const int h = colH[z * Chunk::kSizeX + x];
            if (h <= 0) continue;
            if (h + 1 >= Chunk::kSizeY) continue;
            if (chunk.get(x, h, z) != Block::Grass) continue;
            if (chunk.get(x, h + 1, z) != Block::Air) continue;

            const int worldX = chunkX * Chunk::kSizeX + x;
            const int worldZ = chunkZ * Chunk::kSizeZ + z;
            const Biome biome = biomeAt(worldX, worldZ, seed_);
            const uint64_t hashF = hashCoord(worldX, worldZ, seed_, 0xF10E);
            const uint64_t hashG = hashCoord(worldX, worldZ, seed_, 0xC2A5);

            if (biomeAllowsFlowers(biome) && (hashF % 150u) == 0u) {
                chunk.set(x, h + 1, z, Block::Flower);
                ++flowers;
            } else if (biomeAllowsTallGrass(biome) && (hashG % 10u) == 0u) {
                chunk.set(x, h + 1, z, Block::TallGrass);
                ++grasses;
            }
        }
    }

    if (flowers > 0 || grasses > 0) {
        spdlog::info("Chunk ({},{}) 装饰 花={} 高草={}", chunkX, chunkZ, flowers, grasses);
    }
}

void TerrainGenerator::placeSugarCane(Chunk& chunk, int chunkX, int chunkZ, const int* colH) const {
    int count = 0;
    for (int z = 1; z < Chunk::kSizeZ - 1; ++z) {
        for (int x = 1; x < Chunk::kSizeX - 1; ++x) {
            const int h = colH[z * Chunk::kSizeX + x];
            if (h <= 0) continue;
            if (h + 3 >= Chunk::kSizeY) continue;
            const Block ground = chunk.get(x, h, z);
            if (ground != Block::Grass && ground != Block::Sand && ground != Block::Dirt) continue;
            if (chunk.get(x, h + 1, z) != Block::Air) continue;

            const bool nearWater =
                chunk.get(x - 1, h, z) == Block::Water ||
                chunk.get(x + 1, h, z) == Block::Water ||
                chunk.get(x, h, z - 1) == Block::Water ||
                chunk.get(x, h, z + 1) == Block::Water;
            if (!nearWater) continue;

            const int worldX = chunkX * Chunk::kSizeX + x;
            const int worldZ = chunkZ * Chunk::kSizeZ + z;
            const uint64_t hash = hashCoord(worldX, worldZ, seed_, 0x5C0Fu);
            if ((hash % 8u) != 0u) continue;

            const int height = 2 + static_cast<int>((hash >> 8) % 2u);
            for (int dy = 1; dy <= height; ++dy) {
                if (chunk.get(x, h + dy, z) != Block::Air) break;
                chunk.set(x, h + dy, z, Block::SugarCane);
            }
            ++count;
        }
    }
    if (count > 0) {
        spdlog::info("Chunk ({},{}) 甘蔗 {} 株", chunkX, chunkZ, count);
    }
}

void TerrainGenerator::placeWaterDecorations(Chunk& chunk, int chunkX, int chunkZ) const {
    int lily = 0;
    int coral = 0;
    int kelp = 0;
    for (int z = 0; z < Chunk::kSizeZ; ++z) {
        for (int x = 0; x < Chunk::kSizeX; ++x) {
            int waterTop = -1;
            int waterBottom = -1;
            for (int y = Chunk::kSizeY - 1; y >= 0; --y) {
                if (chunk.get(x, y, z) == Block::Water) {
                    if (waterTop < 0) waterTop = y;
                    waterBottom = y;
                } else if (waterTop >= 0) {
                    break;
                }
            }
            if (waterTop < 0) continue;

            const int worldX = chunkX * Chunk::kSizeX + x;
            const int worldZ = chunkZ * Chunk::kSizeZ + z;
            const uint64_t hashL = hashCoord(worldX, worldZ, seed_, 0x111Au);
            const uint64_t hashC = hashCoord(worldX, worldZ, seed_, 0xC0A1u);
            const uint64_t hashK = hashCoord(worldX, worldZ, seed_, 0xCE1Bu);

            if (waterTop + 1 < Chunk::kSizeY &&
                chunk.get(x, waterTop + 1, z) == Block::Air &&
                (hashL % 200u) == 0u) {
                chunk.set(x, waterTop + 1, z, Block::LilyPad);
                ++lily;
                continue;
            }

            const int waterDepth = waterTop - waterBottom + 1;
            if (waterDepth < 2) continue;

            const Block bedBlock = waterBottom > 0 ? chunk.get(x, waterBottom - 1, z) : Block::Air;
            if (bedBlock != Block::Sand && bedBlock != Block::Dirt &&
                bedBlock != Block::Stone) continue;

            if ((hashC % 60u) == 0u) {
                chunk.set(x, waterBottom, z, Block::Coral);
                ++coral;
            } else if ((hashK % 40u) == 0u) {
                const int kelpHeight = 2 + static_cast<int>((hashK >> 8) % 4u);
                for (int dy = 0; dy < kelpHeight && waterBottom + dy <= waterTop; ++dy) {
                    chunk.set(x, waterBottom + dy, z, Block::Kelp);
                    ++kelp;
                }
            }
        }
    }
    if (lily > 0 || coral > 0 || kelp > 0) {
        spdlog::info("Chunk ({},{}) 水下 lily={} coral={} kelp={}",
                     chunkX, chunkZ, lily, coral, kelp);
    }
}

}
