#pragma once

#include <cstdint>

namespace nenet {

class Chunk;

class TerrainGenerator {
public:
    explicit TerrainGenerator(uint64_t seed);

    void generate(Chunk& chunk, int chunkX, int chunkZ) const;

    [[nodiscard]] int seaLevel() const noexcept { return seaLevel_; }
    [[nodiscard]] int baseHeight() const noexcept { return baseHeight_; }

private:
    [[nodiscard]] float sampleHeight01(int worldX, int worldZ) const;
    [[nodiscard]] bool isCave(int worldX, int y, int worldZ) const;
    void placeTrees(Chunk& chunk, int chunkX, int chunkZ, const int* colH) const;
    void placeDecorations(Chunk& chunk, int chunkX, int chunkZ, const int* colH) const;
    void placeSugarCane(Chunk& chunk, int chunkX, int chunkZ, const int* colH) const;
    void placeWaterDecorations(Chunk& chunk, int chunkX, int chunkZ) const;

    uint64_t seed_;
    int seaLevel_{64};
    int baseHeight_{64};
    int amplitude_{40};
    float mountainAmpMul_{4.0f};
    int dirtDepth_{4};
    float frequency_{0.012f};
    int octaves_{5};
    float persistence_{0.5f};
    float lacunarity_{2.0f};

    float caveFrequency_{0.06f};
    float caveYStretch_{1.5f};
    float caveThreshold_{0.55f};
    int surfaceProtect_{4};
    int snowLine_{200};
    int lavaLevel_{22};
};

}
