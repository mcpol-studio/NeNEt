#include "aabb.h"

namespace nenet {

float clipAxisX(const AABB& m, const AABB& b, float motion) {
    if (m.max.y <= b.min.y || m.min.y >= b.max.y) return motion;
    if (m.max.z <= b.min.z || m.min.z >= b.max.z) return motion;
    if (motion > 0.0f && m.max.x <= b.min.x) {
        const float dist = b.min.x - m.max.x;
        if (dist < motion) motion = dist;
    } else if (motion < 0.0f && m.min.x >= b.max.x) {
        const float dist = b.max.x - m.min.x;
        if (dist > motion) motion = dist;
    }
    return motion;
}

float clipAxisY(const AABB& m, const AABB& b, float motion) {
    if (m.max.x <= b.min.x || m.min.x >= b.max.x) return motion;
    if (m.max.z <= b.min.z || m.min.z >= b.max.z) return motion;
    if (motion > 0.0f && m.max.y <= b.min.y) {
        const float dist = b.min.y - m.max.y;
        if (dist < motion) motion = dist;
    } else if (motion < 0.0f && m.min.y >= b.max.y) {
        const float dist = b.max.y - m.min.y;
        if (dist > motion) motion = dist;
    }
    return motion;
}

float clipAxisZ(const AABB& m, const AABB& b, float motion) {
    if (m.max.x <= b.min.x || m.min.x >= b.max.x) return motion;
    if (m.max.y <= b.min.y || m.min.y >= b.max.y) return motion;
    if (motion > 0.0f && m.max.z <= b.min.z) {
        const float dist = b.min.z - m.max.z;
        if (dist < motion) motion = dist;
    } else if (motion < 0.0f && m.min.z >= b.max.z) {
        const float dist = b.max.z - m.min.z;
        if (dist > motion) motion = dist;
    }
    return motion;
}

}
