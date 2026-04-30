#include "block.h"

namespace nenet {

glm::vec3 blockColor(Block b) noexcept {
    switch (b) {
        case Block::Stone:  return {0.55f, 0.55f, 0.55f};
        case Block::Dirt:   return {0.50f, 0.36f, 0.22f};
        case Block::Grass:  return {0.36f, 0.70f, 0.30f};
        case Block::Sand:   return {0.86f, 0.80f, 0.55f};
        case Block::Wood:   return {0.45f, 0.30f, 0.15f};
        case Block::Leaves: return {0.20f, 0.55f, 0.20f};
        case Block::Air:
        default:            return {1.0f, 0.0f, 1.0f};
    }
}

}
