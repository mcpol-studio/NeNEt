#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

namespace nenet {

class BlockRegistry {
public:
    [[nodiscard]] static BlockRegistry& instance();

    bool load(const std::filesystem::path& txtPath);

    [[nodiscard]] int slotByName(const std::string& name) const;

    [[nodiscard]] int slotOr(const std::string& name, const std::string& fallback) const;

    [[nodiscard]] int grid() const noexcept { return grid_; }
    [[nodiscard]] float tileNdc() const noexcept { return 1.0f / static_cast<float>(grid_); }
    [[nodiscard]] glm::ivec2 slotXY(int slot) const noexcept {
        if (grid_ <= 0) return {0, 0};
        return {slot % grid_, slot / grid_};
    }

private:
    BlockRegistry() = default;
    int grid_{64};
    std::unordered_map<std::string, int> nameToSlot_;
};

}
