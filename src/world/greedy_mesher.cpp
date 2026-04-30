#include "greedy_mesher.h"

#include "block.h"
#include "chunk.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdint>
#include <vector>

namespace nenet {

namespace {

struct MaskEntry {
    Block block{Block::Air};
    bool positive{false};
    [[nodiscard]] bool isFace() const noexcept { return block != Block::Air; }
    bool operator==(const MaskEntry& o) const noexcept {
        return block == o.block && positive == o.positive;
    }
};

glm::vec3 shadeForNormal(const glm::ivec3& n) {
    if (n.y > 0) return glm::vec3(1.00f);
    if (n.y < 0) return glm::vec3(0.55f);
    if (n.x != 0) return glm::vec3(0.85f);
    return glm::vec3(0.70f);
}

}

ChunkMeshData GreedyMesher::mesh(const Chunk& chunk) {
    const auto t0 = std::chrono::steady_clock::now();

    ChunkMeshData out;
    out.vertices.reserve(2048);
    out.indices.reserve(3072);

    const int kSize[3] = {Chunk::kSizeX, Chunk::kSizeY, Chunk::kSizeZ};

    for (int d = 0; d < 3; ++d) {
        const int u = (d + 1) % 3;
        const int v = (d + 2) % 3;

        const int sizeU = kSize[u];
        const int sizeV = kSize[v];
        const int sizeD = kSize[d];

        glm::ivec3 du(0);
        glm::ivec3 dv(0);
        du[u] = 1;
        dv[v] = 1;

        std::vector<MaskEntry> mask(static_cast<size_t>(sizeU) * sizeV);

        for (int i = -1; i < sizeD; ++i) {
            for (int b = 0; b < sizeV; ++b) {
                for (int a = 0; a < sizeU; ++a) {
                    glm::ivec3 p1(0), p2(0);
                    p1[d] = i;
                    p1[u] = a;
                    p1[v] = b;
                    p2[d] = i + 1;
                    p2[u] = a;
                    p2[v] = b;

                    const Block bk1 = (i >= 0) ? chunk.get(p1.x, p1.y, p1.z) : Block::Air;
                    const Block bk2 = (i + 1 < sizeD) ? chunk.get(p2.x, p2.y, p2.z) : Block::Air;

                    const bool s1 = isSolid(bk1);
                    const bool s2 = isSolid(bk2);

                    MaskEntry m{};
                    if (s1 && !s2) {
                        m.block = bk1;
                        m.positive = true;
                    } else if (!s1 && s2) {
                        m.block = bk2;
                        m.positive = false;
                    }
                    mask[a + b * sizeU] = m;
                }
            }

            for (int b = 0; b < sizeV; ++b) {
                int a = 0;
                while (a < sizeU) {
                    MaskEntry cell = mask[a + b * sizeU];
                    if (!cell.isFace()) {
                        ++a;
                        continue;
                    }

                    int w = 1;
                    while (a + w < sizeU && mask[(a + w) + b * sizeU] == cell) ++w;

                    int h = 1;
                    bool extend = true;
                    while (b + h < sizeV && extend) {
                        for (int k = 0; k < w; ++k) {
                            if (!(mask[(a + k) + (b + h) * sizeU] == cell)) {
                                extend = false;
                                break;
                            }
                        }
                        if (extend) ++h;
                    }

                    glm::ivec3 normal(0);
                    normal[d] = cell.positive ? 1 : -1;

                    glm::vec3 origin(0.0f);
                    origin[d] = static_cast<float>(i + 1);
                    origin[u] = static_cast<float>(a);
                    origin[v] = static_cast<float>(b);

                    const glm::vec3 dU = glm::vec3(du) * static_cast<float>(w);
                    const glm::vec3 dV = glm::vec3(dv) * static_cast<float>(h);

                    const glm::vec3 color = blockColor(cell.block) * shadeForNormal(normal);
                    const uint32_t base = static_cast<uint32_t>(out.vertices.size());

                    if (cell.positive) {
                        out.vertices.push_back({origin,           color});
                        out.vertices.push_back({origin + dU,      color});
                        out.vertices.push_back({origin + dU + dV, color});
                        out.vertices.push_back({origin + dV,      color});
                    } else {
                        out.vertices.push_back({origin,           color});
                        out.vertices.push_back({origin + dV,      color});
                        out.vertices.push_back({origin + dU + dV, color});
                        out.vertices.push_back({origin + dU,      color});
                    }
                    out.indices.push_back(base + 0);
                    out.indices.push_back(base + 1);
                    out.indices.push_back(base + 2);
                    out.indices.push_back(base + 0);
                    out.indices.push_back(base + 2);
                    out.indices.push_back(base + 3);

                    for (int kb = 0; kb < h; ++kb) {
                        for (int ka = 0; ka < w; ++ka) {
                            mask[(a + ka) + (b + kb) * sizeU] = MaskEntry{};
                        }
                    }

                    a += w;
                }
            }
        }
    }

    const auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    spdlog::info("GreedyMesher 完成 verts={} idx={} 耗时={:.2f}ms",
                 out.vertices.size(), out.indices.size(), ms);
    return out;
}

}
