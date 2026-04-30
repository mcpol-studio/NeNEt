#pragma once

#include "../render/vertex.h"

#include <cstdint>
#include <vector>

namespace nenet {

class Chunk;

struct ChunkMeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

class NaiveMesher {
public:
    [[nodiscard]] static ChunkMeshData mesh(const Chunk& chunk);
};

}
