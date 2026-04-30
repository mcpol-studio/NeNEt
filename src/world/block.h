#pragma once

#include <cstdint>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

namespace nenet {

enum class Block : uint8_t {
    Air = 0,
    Stone,
    Dirt,
    Grass,
    Sand,
    Wood,
    Leaves,
};

[[nodiscard]] constexpr bool isSolid(Block b) noexcept {
    return b != Block::Air;
}

[[nodiscard]] glm::vec3 blockColor(Block b) noexcept;

}
