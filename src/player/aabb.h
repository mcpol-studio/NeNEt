#pragma once

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

namespace nenet {

struct AABB {
    glm::vec3 min;
    glm::vec3 max;
};

[[nodiscard]] float clipAxisX(const AABB& moving, const AABB& blocker, float motion);
[[nodiscard]] float clipAxisY(const AABB& moving, const AABB& blocker, float motion);
[[nodiscard]] float clipAxisZ(const AABB& moving, const AABB& blocker, float motion);

}
