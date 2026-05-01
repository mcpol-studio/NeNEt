#pragma once

#include "block.h"

#include <cstdint>

namespace nenet {

enum class Biome : uint8_t {
    Plains,
    Snowy,
    Desert,
    Mountain,
    Mesa,
};

[[nodiscard]] float temperatureNoise(int worldX, int worldZ, uint64_t seed) noexcept;
[[nodiscard]] float continentalNoise(int worldX, int worldZ, uint64_t seed) noexcept;
[[nodiscard]] float biomeRawNoise(int worldX, int worldZ, uint64_t seed) noexcept;
[[nodiscard]] Biome biomeAt(int worldX, int worldZ, uint64_t seed) noexcept;

[[nodiscard]] Block biomeTopMat(Biome b) noexcept;
[[nodiscard]] Block biomeSubMat(Biome b) noexcept;

[[nodiscard]] inline bool biomeAllowsTrees(Biome b) noexcept {
    return b == Biome::Plains;
}

[[nodiscard]] inline bool biomeAllowsFlowers(Biome b) noexcept {
    return b == Biome::Plains;
}

[[nodiscard]] inline bool biomeAllowsTallGrass(Biome b) noexcept {
    return b == Biome::Plains;
}

}
