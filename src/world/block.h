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
    Water,
    Snow,
    Lava,
    Flower,
    TallGrass,
    MesaRed,
    MesaOrange,
    MesaYellow,
    MesaWhite,
    SugarCane,
    LilyPad,
    Coral,
    Kelp,
    Ice,
    Cactus,
};

[[nodiscard]] constexpr bool isSolid(Block b) noexcept {
    return b != Block::Air;
}

[[nodiscard]] constexpr bool isHittable(Block b) noexcept {
    return b != Block::Air && b != Block::Water && b != Block::Lava;
}

[[nodiscard]] constexpr bool hasGravity(Block b) noexcept {
    return b == Block::Sand;
}

[[nodiscard]] constexpr bool isCrossPlant(Block b) noexcept {
    return b == Block::Flower || b == Block::TallGrass ||
           b == Block::SugarCane || b == Block::LilyPad ||
           b == Block::Coral || b == Block::Kelp;
}

[[nodiscard]] constexpr int transparencyLevel(Block b) noexcept {
    if (b == Block::Air) return 2;
    if (b == Block::Water || b == Block::Lava ||
        b == Block::Flower || b == Block::TallGrass ||
        b == Block::SugarCane || b == Block::LilyPad ||
        b == Block::Coral || b == Block::Kelp) return 1;
    return 0;
}

[[nodiscard]] glm::vec3 blockColor(Block b) noexcept;

enum class Face : uint8_t { PosX = 0, NegX, PosY, NegY, PosZ, NegZ };

[[nodiscard]] glm::ivec2 atlasSlot(Block b, Face f) noexcept;

}
