#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace nenet {

class VulkanContext;

class Swapchain {
public:
    Swapchain(VulkanContext& ctx, uint32_t width, uint32_t height);
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    [[nodiscard]] VkSwapchainKHR handle() const noexcept { return swapchain_; }
    [[nodiscard]] VkFormat imageFormat() const noexcept { return imageFormat_; }
    [[nodiscard]] VkExtent2D extent() const noexcept { return extent_; }
    [[nodiscard]] uint32_t imageCount() const noexcept { return static_cast<uint32_t>(images_.size()); }
    [[nodiscard]] VkImage image(uint32_t index) const { return images_[index]; }
    [[nodiscard]] VkImageView imageView(uint32_t index) const { return imageViews_[index]; }

private:
    VulkanContext& ctx_;
    VkSwapchainKHR swapchain_{VK_NULL_HANDLE};
    VkFormat imageFormat_{VK_FORMAT_UNDEFINED};
    VkExtent2D extent_{};
    std::vector<VkImage> images_;
    std::vector<VkImageView> imageViews_;
};

}
