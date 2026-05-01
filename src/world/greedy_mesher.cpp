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

                    const int l1 = transparencyLevel(bk1);
                    const int l2 = transparencyLevel(bk2);

                    MaskEntry m{};
                    if (bk1 != Block::Air && !isCrossPlant(bk1) && l2 > l1) {
                        m.block = bk1;
                        m.positive = true;
                    } else if (bk2 != Block::Air && !isCrossPlant(bk2) && l1 > l2) {
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

                    const glm::vec3 color = shadeForNormal(normal);

                    Face faceEnum;
                    if (d == 0) faceEnum = cell.positive ? Face::PosX : Face::NegX;
                    else if (d == 1) faceEnum = cell.positive ? Face::PosY : Face::NegY;
                    else faceEnum = cell.positive ? Face::PosZ : Face::NegZ;
                    const glm::ivec2 slot = atlasSlot(cell.block, faceEnum);
                    const glm::vec2 atlasOrigin = glm::vec2(slot) * (1.0f / 64.0f);

                    const float fw = static_cast<float>(w);
                    const float fh = static_cast<float>(h);
                    glm::vec2 uv00, uv10, uv11, uv01;
                    if (d == 0) {
                        uv00 = glm::vec2(0.0f, fw);
                        uv10 = glm::vec2(0.0f, 0.0f);
                        uv11 = glm::vec2(fh,   0.0f);
                        uv01 = glm::vec2(fh,   fw);
                    } else if (d == 1) {
                        uv00 = glm::vec2(0.0f, 0.0f);
                        uv10 = glm::vec2(0.0f, fw);
                        uv11 = glm::vec2(fh,   fw);
                        uv01 = glm::vec2(fh,   0.0f);
                    } else {
                        uv00 = glm::vec2(0.0f, fh);
                        uv10 = glm::vec2(fw,   fh);
                        uv11 = glm::vec2(fw,   0.0f);
                        uv01 = glm::vec2(0.0f, 0.0f);
                    }

                    const uint32_t base = static_cast<uint32_t>(out.vertices.size());

                    if (cell.positive) {
                        out.vertices.push_back({origin,           color, uv00, atlasOrigin});
                        out.vertices.push_back({origin + dU,      color, uv10, atlasOrigin});
                        out.vertices.push_back({origin + dU + dV, color, uv11, atlasOrigin});
                        out.vertices.push_back({origin + dV,      color, uv01, atlasOrigin});
                    } else {
                        out.vertices.push_back({origin,           color, uv00, atlasOrigin});
                        out.vertices.push_back({origin + dV,      color, uv01, atlasOrigin});
                        out.vertices.push_back({origin + dU + dV, color, uv11, atlasOrigin});
                        out.vertices.push_back({origin + dU,      color, uv10, atlasOrigin});
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

    for (int y = 0; y < Chunk::kSizeY; ++y) {
        for (int z = 0; z < Chunk::kSizeZ; ++z) {
            for (int x = 0; x < Chunk::kSizeX; ++x) {
                const Block bk = chunk.get(x, y, z);
                if (!isCrossPlant(bk)) continue;

                const glm::ivec2 slot = atlasSlot(bk, Face::PosY);
                const glm::vec2 atlasOrigin = glm::vec2(slot) * (1.0f / 64.0f);
                const glm::vec3 color(1.0f);
                const float fx = static_cast<float>(x);
                const float fy = static_cast<float>(y);
                const float fz = static_cast<float>(z);

                struct Quad {
                    glm::vec3 a, b, c, d;
                };
                const Quad q1{
                    {fx + 0.0f, fy + 0.0f, fz + 0.0f},
                    {fx + 1.0f, fy + 0.0f, fz + 1.0f},
                    {fx + 1.0f, fy + 1.0f, fz + 1.0f},
                    {fx + 0.0f, fy + 1.0f, fz + 0.0f},
                };
                const Quad q2{
                    {fx + 1.0f, fy + 0.0f, fz + 0.0f},
                    {fx + 0.0f, fy + 0.0f, fz + 1.0f},
                    {fx + 0.0f, fy + 1.0f, fz + 1.0f},
                    {fx + 1.0f, fy + 1.0f, fz + 0.0f},
                };

                for (const Quad& q : {q1, q2}) {
                    const uint32_t base = static_cast<uint32_t>(out.vertices.size());
                    out.vertices.push_back({q.a, color, {0.0f, 1.0f}, atlasOrigin});
                    out.vertices.push_back({q.b, color, {1.0f, 1.0f}, atlasOrigin});
                    out.vertices.push_back({q.c, color, {1.0f, 0.0f}, atlasOrigin});
                    out.vertices.push_back({q.d, color, {0.0f, 0.0f}, atlasOrigin});
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
    spdlog::info("GreedyMesher 完成 verts={} idx={} 耗时={:.2f}ms",
                 out.vertices.size(), out.indices.size(), ms);
    return out;
}

}
