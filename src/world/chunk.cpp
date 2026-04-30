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
            for (int y = 0; y < 4; ++y) {
                set(x, y, z, Block::Dirt);
            }
            set(x, 4, z, Block::Grass);
        }
    }

    for (int y = 5; y <= 7; ++y) set(2, y, 2, Block::Stone);

    for (int y = 5; y <= 6; ++y) set(8, y, 8, Block::Sand);

    for (int y = 5; y <= 6; ++y) set(13, y, 5, Block::Wood);
    for (int y = 7; y <= 8; ++y) set(13, y, 5, Block::Leaves);

    set(5, 5, 11, Block::Stone);
    set(6, 5, 11, Block::Stone);
    set(7, 5, 11, Block::Stone);
    set(6, 6, 11, Block::Stone);

    set(11, 5, 2, Block::Sand);
    set(12, 5, 2, Block::Sand);
    set(11, 5, 3, Block::Sand);
    set(12, 5, 3, Block::Sand);
}

}
