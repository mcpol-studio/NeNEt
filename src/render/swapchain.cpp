#include "swapchain.h"

#include "vulkan_context.h"

#include <VkBootstrap.h>
#include <spdlog/spdlog.h>

#include <stdexcept>

namespace nenet {

Swapchain::Swapchain(VulkanContext& ctx, uint32_t width, uint32_t height) : ctx_(ctx) {
    vkb::SwapchainBuilder builder(ctx.physicalDevice(), ctx.device(), ctx.surface(),
                                   static_cast<int32_t>(ctx.graphicsQueueFamily()),
                                   static_cast<int32_t>(ctx.graphicsQueueFamily()));

    auto ret = builder
        .set_desired_format(VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        .build();

    if (!ret) {
        throw std::runtime_error("Swapchain 创建失败: " + ret.error().message());
    }

    vkb::Swapchain vkbSc = ret.value();
    swapchain_ = vkbSc.swapchain;
    imageFormat_ = vkbSc.image_format;
    extent_ = vkbSc.extent;
    images_ = vkbSc.get_images().value();
    imageViews_ = vkbSc.get_image_views().value();

    spdlog::info("Swapchain 已创建 {}x{} format={} images={}",
                 extent_.width, extent_.height,
                 static_cast<int>(imageFormat_), images_.size());
}

Swapchain::~Swapchain() {
    for (VkImageView view : imageViews_) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(ctx_.device(), view, nullptr);
        }
    }
    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(ctx_.device(), swapchain_, nullptr);
    }
    spdlog::info("Swapchain 已销毁");
}

}
