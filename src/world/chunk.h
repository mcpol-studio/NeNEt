#pragma once

#include "block.h"

#include <array>

namespace nenet {

class Chunk {
public:
    static constexpr int kSizeX = 16;
    static constexpr int kSizeY = 16;
    static constexpr int kSizeZ = 16;
    static constexpr int kVolume = kSizeX * kSizeY * kSizeZ;

    [[nodiscard]] Block get(int x, int y, int z) const noexcept;
    void set(int x, int y, int z, Block b) noexcept;

    [[nodiscard]] static constexpr bool inBounds(int x, int y, int z) noexcept {
        return x >= 0 && x < kSizeX && y >= 0 && y < kSizeY && z >= 0 && z < kSizeZ;
    }

    void fillTestTerrain();

private:
    std::array<Block, kVolume> blocks_{};

    [[nodiscard]] static constexpr int index(int x, int y, int z) noexcept {
        return x + kSizeX * (z + kSizeZ * y);
    }
};

}
