#include "chunk_mesher.h"

#include "block.h"
#include "chunk.h"

#include <spdlog/spdlog.h>

#include <chrono>

namespace nenet {

namespace {

struct FaceTemplate {
    glm::vec3 corners[4];
    glm::ivec3 normal;
};

constexpr FaceTemplate kFaces[6] = {
    {{{1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}}, {1, 0, 0}},
    {{{0, 0, 1}, {0, 1, 1}, {0, 1, 0}, {0, 0, 0}}, {-1, 0, 0}},
    {{{0, 1, 0}, {0, 1, 1}, {1, 1, 1}, {1, 1, 0}}, {0, 1, 0}},
    {{{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}}, {0, -1, 0}},
    {{{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}, {0, 0, 1}},
    {{{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}}, {0, 0, -1}},
};

glm::vec3 shadeForNormal(const glm::ivec3& n) {
    if (n.y > 0) return glm::vec3(1.00f);
    if (n.y < 0) return glm::vec3(0.55f);
    if (n.x != 0) return glm::vec3(0.85f);
    return glm::vec3(0.70f);
}

}

ChunkMeshData NaiveMesher::mesh(const Chunk& chunk) {
    const auto t0 = std::chrono::steady_clock::now();

    ChunkMeshData out;
    out.vertices.reserve(2048);
    out.indices.reserve(3072);

    for (int y = 0; y < Chunk::kSizeY; ++y) {
        for (int z = 0; z < Chunk::kSizeZ; ++z) {
            for (int x = 0; x < Chunk::kSizeX; ++x) {
                const Block b = chunk.get(x, y, z);
                if (!isSolid(b)) continue;

                const glm::vec3 origin(x, y, z);
                const glm::vec3 baseColor = blockColor(b);

                for (const FaceTemplate& face : kFaces) {
                    const int nx = x + face.normal.x;
                    const int ny = y + face.normal.y;
                    const int nz = z + face.normal.z;
                    if (Chunk::inBounds(nx, ny, nz) && isSolid(chunk.get(nx, ny, nz))) {
                        continue;
                    }

                    const glm::vec3 shaded = baseColor * shadeForNormal(face.normal);
                    const uint32_t base = static_cast<uint32_t>(out.vertices.size());

                    for (const glm::vec3& corner : face.corners) {
                        out.vertices.push_back({origin + corner, shaded, {0.0f, 0.0f}, {0.0f, 0.0f}});
                    }
                    out.indices.push_back(base + 0);
                    out.indices.push_back(base + 1);
                    out.indices.push_back(base + 2);
                    out.indices.push_back(base + 0);
                    out.indices.push_back(base + 2);
                    out.indices.push_back(base + 3);
                }
            }
        }
    }

    const auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    spdlog::info("NaiveMesher 完成 verts={} idx={} 耗时={:.2f}ms",
                 out.vertices.size(), out.indices.size(), ms);
    return out;
}

}
