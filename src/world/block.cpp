#include "block.h"

#include "block_registry.h"

namespace nenet {

namespace {

struct FaceNames {
    const char* posY;
    const char* negY;
    const char* side;
    const char* fallback;
};

FaceNames namesFor(Block b) {
    switch (b) {
        case Block::Stone:    return {"stone", "stone", "stone", "stone"};
        case Block::Dirt:     return {"dirt", "dirt", "dirt", "dirt"};
        case Block::Grass:    return {"grass_block_top", "dirt", "grass_block_side", "dirt"};
        case Block::Sand:     return {"sand", "sand", "sand", "sand"};
        case Block::Wood:     return {"oak_log_top", "oak_log_top", "oak_log", "oak_log"};
        case Block::Leaves:   return {"oak_leaves", "oak_leaves", "oak_leaves", "oak_leaves"};
        case Block::Water:    return {"water_still", "water_still", "water_still", "water_still"};
        case Block::Snow:     return {"snow", "snow", "snow", "snow"};
        case Block::Lava:     return {"lava_still", "lava_still", "lava_still", "lava_still"};
        case Block::Flower:   return {"poppy", "poppy", "poppy", "poppy"};
        case Block::TallGrass:return {"short_grass", "short_grass", "short_grass", "short_grass"};
        case Block::MesaRed:    return {"red_terracotta", "red_terracotta", "red_terracotta", "red_terracotta"};
        case Block::MesaOrange: return {"orange_terracotta", "orange_terracotta", "orange_terracotta", "orange_terracotta"};
        case Block::MesaYellow: return {"yellow_terracotta", "yellow_terracotta", "yellow_terracotta", "yellow_terracotta"};
        case Block::MesaWhite:  return {"white_terracotta", "white_terracotta", "white_terracotta", "white_terracotta"};
        case Block::SugarCane:  return {"sugar_cane", "sugar_cane", "sugar_cane", "sugar_cane"};
        case Block::LilyPad:    return {"lily_pad", "lily_pad", "lily_pad", "lily_pad"};
        case Block::Coral:      return {"fire_coral", "fire_coral", "fire_coral", "fire_coral"};
        case Block::Kelp:       return {"kelp", "kelp", "kelp", "kelp"};
        case Block::Ice:        return {"ice", "ice", "ice", "ice"};
        case Block::Cactus:     return {"cactus_top", "cactus_bottom", "cactus_side", "cactus_side"};
        case Block::Air:
        default:                return {"air", "air", "air", "air"};
    }
}

}

glm::ivec2 atlasSlot(Block b, Face f) noexcept {
    const auto names = namesFor(b);
    const char* name;
    switch (f) {
        case Face::PosY: name = names.posY; break;
        case Face::NegY: name = names.negY; break;
        default:         name = names.side; break;
    }
    const auto& reg = BlockRegistry::instance();
    int slot = reg.slotByName(name);
    if (slot < 0) slot = reg.slotByName(names.fallback);
    if (slot < 0) slot = 0;
    return reg.slotXY(slot);
}

glm::vec3 blockColor(Block b) noexcept {
    switch (b) {
        case Block::Stone:  return {0.55f, 0.55f, 0.55f};
        case Block::Dirt:   return {0.50f, 0.36f, 0.22f};
        case Block::Grass:  return {0.36f, 0.70f, 0.30f};
        case Block::Sand:   return {0.86f, 0.80f, 0.55f};
        case Block::Wood:   return {0.45f, 0.30f, 0.15f};
        case Block::Leaves: return {0.20f, 0.55f, 0.20f};
        case Block::Water:  return {0.18f, 0.45f, 0.82f};
        case Block::Snow:   return {0.92f, 0.96f, 0.98f};
        case Block::Lava:   return {0.95f, 0.40f, 0.12f};
        case Block::Flower: return {0.86f, 0.31f, 0.39f};
        case Block::TallGrass: return {0.51f, 0.78f, 0.31f};
        case Block::MesaRed:    return {0.71f, 0.27f, 0.20f};
        case Block::MesaOrange: return {0.78f, 0.51f, 0.27f};
        case Block::MesaYellow: return {0.86f, 0.75f, 0.39f};
        case Block::MesaWhite:  return {0.92f, 0.86f, 0.78f};
        case Block::SugarCane:  return {0.43f, 0.71f, 0.35f};
        case Block::LilyPad:    return {0.27f, 0.55f, 0.24f};
        case Block::Coral:      return {0.30f, 0.55f, 0.85f};
        case Block::Kelp:       return {0.16f, 0.55f, 0.31f};
        case Block::Ice:        return {0.71f, 0.90f, 0.96f};
        case Block::Cactus:     return {0.36f, 0.55f, 0.27f};
        case Block::Air:
        default:            return {1.0f, 0.0f, 1.0f};
    }
}

}
