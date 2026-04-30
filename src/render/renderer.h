#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace nenet {

class VulkanContext;
class Swapchain;
class CubePipeline;
class DepthImage;
class Camera;
class ChunkMesh;

class Renderer {
public:
    Renderer(VulkanContext& ctx, Swapchain& swapchain, Camera& camera, ChunkMesh& mesh);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void drawFrame();

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
    ChunkMesh& mesh_;
    std::unique_ptr<CubePipeline> pipeline_;
    std::unique_ptr<DepthImage> depth_;
    std::array<FrameData, kFramesInFlight> frames_;
    std::vector<VkSemaphore> renderFinishedPerImage_;
    uint32_t currentFrame_{0};
};

}
