#include "chunk.h"

namespace nenet {

Block Chunk::get(int x, int y, int z) const noexcept {
    if (!inBounds(x, y, z)) return Block::Air;
    return blocks_[index(x, y, z)];
}

void Chunk::set(int x, int y, int z, Block b) noexcept {
    if (!inBounds(x, y, z)) return;
    blocks_[index(x, y, z)] = b;
}

void Chunk::fillTestTerrain() {
    for (int z = 0; z < kSizeZ; ++z) {
        for (int x = 0; x < kSizeX; ++x) {
            for (int y = 0; y < 4; ++y) set(x, y, z, Block::Dirt);
            set(x, 4, z, Block::Grass);
        }
    }
}

}
