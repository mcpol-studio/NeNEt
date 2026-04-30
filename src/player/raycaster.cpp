#include "raycaster.h"

#include <cmath>
#include <limits>

namespace nenet {

std::optional<RaycastHit> Raycaster::cast(const BlockReader& reader,
                                           const glm::vec3& origin,
                                           const glm::vec3& direction,
                                           float maxDistance) {
    if (glm::length(direction) < 1e-6f) return std::nullopt;
    const glm::vec3 d = glm::normalize(direction);

    int x = static_cast<int>(std::floor(origin.x));
    int y = static_cast<int>(std::floor(origin.y));
    int z = static_cast<int>(std::floor(origin.z));

    const int sx = d.x > 0.0f ? 1 : (d.x < 0.0f ? -1 : 0);
    const int sy = d.y > 0.0f ? 1 : (d.y < 0.0f ? -1 : 0);
    const int sz = d.z > 0.0f ? 1 : (d.z < 0.0f ? -1 : 0);

    constexpr float kInf = std::numeric_limits<float>::infinity();

    const auto initTMax = [](int s, float originAxis, float dAxis) -> float {
        if (s == 0) return kInf;
        const float fl = std::floor(originAxis);
        const float frac = (s > 0) ? (fl + 1.0f - originAxis) : (originAxis - fl);
        return frac / std::abs(dAxis);
    };
    const auto initDelta = [](int s, float dAxis) -> float {
        return s == 0 ? kInf : 1.0f / std::abs(dAxis);
    };

    float tMaxX = initTMax(sx, origin.x, d.x);
    float tMaxY = initTMax(sy, origin.y, d.y);
    float tMaxZ = initTMax(sz, origin.z, d.z);
    const float tDx = initDelta(sx, d.x);
    const float tDy = initDelta(sy, d.y);
    const float tDz = initDelta(sz, d.z);

    glm::ivec3 normal{0, 1, 0};
    float t = 0.0f;

    if (isSolid(reader(x, y, z))) {
        return RaycastHit{glm::ivec3{x, y, z}, normal, 0.0f};
    }

    while (t < maxDistance) {
        if (tMaxX < tMaxY && tMaxX < tMaxZ) {
            x += sx;
            t = tMaxX;
            tMaxX += tDx;
            normal = glm::ivec3(-sx, 0, 0);
        } else if (tMaxY < tMaxZ) {
            y += sy;
            t = tMaxY;
            tMaxY += tDy;
            normal = glm::ivec3(0, -sy, 0);
        } else {
            z += sz;
            t = tMaxZ;
            tMaxZ += tDz;
            normal = glm::ivec3(0, 0, -sz);
        }

        if (t > maxDistance) break;

        if (isSolid(reader(x, y, z))) {
            return RaycastHit{glm::ivec3{x, y, z}, normal, t};
        }
    }

    return std::nullopt;
}

}
