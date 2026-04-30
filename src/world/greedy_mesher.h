#pragma once

#include "chunk_mesher.h"

namespace nenet {

class GreedyMesher {
public:
    [[nodiscard]] static ChunkMeshData mesh(const Chunk& chunk);
};

}
