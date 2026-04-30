#pragma once

#include "../world/block.h"

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include <functional>
#include <optional>

namespace nenet {

struct RaycastHit {
    glm::ivec3 blockPos;
    glm::ivec3 normal;
    float distance;
};

using BlockReader = std::function<Block(int, int, int)>;

class Raycaster {
public:
    [[nodiscard]] static std::optional<RaycastHit> cast(const BlockReader& reader,
                                                         const glm::vec3& origin,
                                                         const glm::vec3& direction,
                                                         float maxDistance);
};

}
