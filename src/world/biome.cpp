#include "biome.h"

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/noise.hpp>

namespace nenet {

namespace {

constexpr float kTempFreq = 0.0035f;
constexpr float kContFreq = 0.0055f;

}

float temperatureNoise(int worldX, int worldZ, uint64_t seed) noexcept {
    const float ox = static_cast<float>(seed & 0xFFFFu) * 0.043f;
    const float oz = static_cast<float>((seed >> 16) & 0xFFFFu) * 0.071f;
    return glm::perlin(glm::vec2(
        worldX * kTempFreq + ox,
        worldZ * kTempFreq + oz));
}

float continentalNoise(int worldX, int worldZ, uint64_t seed) noexcept {
    const uint64_t mixed = seed ^ 0xDEADBEEFCAFEBABEull;
    const float ox = static_cast<float>(mixed & 0xFFFFu) * 0.057f;
    const float oz = static_cast<float>((mixed >> 16) & 0xFFFFu) * 0.083f;
    return glm::perlin(glm::vec2(
        worldX * kContFreq + ox,
        worldZ * kContFreq + oz));
}

float biomeRawNoise(int worldX, int worldZ, uint64_t seed) noexcept {
    return continentalNoise(worldX, worldZ, seed);
}

Biome biomeAt(int worldX, int worldZ, uint64_t seed) noexcept {
    const float t = temperatureNoise(worldX, worldZ, seed);
    const float c = continentalNoise(worldX, worldZ, seed);

    if (c >  0.50f) return Biome::Mountain;
    if (t < -0.40f) return Biome::Snowy;
    if (t >  0.30f && c >  0.10f) return Biome::Mesa;
    if (t >  0.30f && c <  0.00f) return Biome::Desert;
    return Biome::Plains;
}

Block biomeTopMat(Biome b) noexcept {
    switch (b) {
        case Biome::Snowy:    return Block::Snow;
        case Biome::Desert:   return Block::Sand;
        case Biome::Mountain: return Block::Stone;
        case Biome::Mesa:     return Block::MesaRed;
        case Biome::Plains:
        default:              return Block::Grass;
    }
}

Block biomeSubMat(Biome b) noexcept {
    switch (b) {
        case Biome::Desert:   return Block::Sand;
        case Biome::Mountain: return Block::Stone;
        case Biome::Mesa:     return Block::Stone;
        case Biome::Snowy:
        case Biome::Plains:
        default:              return Block::Dirt;
    }
}

}
