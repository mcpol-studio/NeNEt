#pragma once

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace nenet {

class VulkanContext;
class Swapchain;
class CubePipeline;
class HotbarPipeline;
class MenuPipeline;
class HudPipeline;
class HudRenderer;
class OutlinePipeline;
class WallpaperPipeline;
class Texture;
class DepthImage;
class Camera;
class ChunkManager;
class ParticleSystem;
class FallingBlocks;

class Renderer {
public:
    Renderer(VulkanContext& ctx, Swapchain& swapchain, Camera& camera,
             ChunkManager& manager, ParticleSystem& particles, FallingBlocks& falling,
             const std::optional<std::filesystem::path>& wallpaperPath = std::nullopt);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void drawFrame();
    void setHotbar(int selectedSlot, const std::array<glm::vec4, 6>& colors) noexcept;
    void setMenuVisible(bool visible, int hoverIndex, bool drawButtons = true) noexcept;
    [[nodiscard]] HudRenderer& hud() noexcept { return *hud_; }
    void setHudVisible(bool visible) noexcept { hudVisible_ = visible; }
    void setSelectedBlock(bool has, const glm::ivec3& pos) noexcept;
    void setUnderwater(bool b) noexcept { underwater_ = b; }

    static constexpr uint32_t kFramesInFlight = 2;

private:
    struct FrameData {
        VkCommandPool commandPool{VK_NULL_HANDLE};
        VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
        VkSemaphore imageAvailable{VK_NULL_HANDLE};
        VkFence inFlight{VK_NULL_HANDLE};
    };

    void createFrames();
    void destroyFrames();
    void createPerImageSemaphores();
    void destroyPerImageSemaphores();
    void recordCommand(VkCommandBuffer cmd, uint32_t imageIndex);
    static void transitionColor(VkCommandBuffer cmd, VkImage image,
                                VkImageLayout oldLayout, VkImageLayout newLayout);
    static void transitionDepth(VkCommandBuffer cmd, VkImage image);

    VulkanContext& ctx_;
    Swapchain& swapchain_;
    Camera& camera_;
    ChunkManager& manager_;
    ParticleSystem& particles_;
    FallingBlocks& falling_;
    std::unique_ptr<CubePipeline> pipeline_;
    std::unique_ptr<HotbarPipeline> hotbarPipeline_;
    std::unique_ptr<MenuPipeline> menuPipeline_;
    std::unique_ptr<HudPipeline> hudPipeline_;
    std::unique_ptr<HudRenderer> hud_;
    std::unique_ptr<OutlinePipeline> outlinePipeline_;
    std::unique_ptr<WallpaperPipeline> wallpaperPipeline_;
    std::unique_ptr<Texture> atlasTexture_;
    std::unique_ptr<Texture> wallpaperTexture_;
    VkDescriptorPool descriptorPool_{VK_NULL_HANDLE};
    VkDescriptorSet atlasDescriptorSet_{VK_NULL_HANDLE};
    VkDescriptorSet wallpaperDescriptorSet_{VK_NULL_HANDLE};
    std::unique_ptr<DepthImage> depth_;
    std::array<FrameData, kFramesInFlight> frames_;
    std::vector<VkSemaphore> renderFinishedPerImage_;
    uint32_t currentFrame_{0};

    std::array<glm::vec4, 6> hotbarColors_{};
    int hotbarSelected_{0};

    bool menuVisible_{false};
    int menuHover_{-1};
    bool menuButtonsVisible_{true};
    bool hudVisible_{false};

    bool hasSelectedBlock_{false};
    glm::ivec3 selectedBlock_{0};
    bool underwater_{false};
};

}
