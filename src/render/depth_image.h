#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <cstdint>

namespace nenet {

class DepthImage {
public:
    DepthImage(VkDevice device, VmaAllocator allocator,
               uint32_t width, uint32_t height, VkFormat format = VK_FORMAT_D32_SFLOAT);
    ~DepthImage();

    DepthImage(const DepthImage&) = delete;
    DepthImage& operator=(const DepthImage&) = delete;

    [[nodiscard]] VkImage image() const noexcept { return image_; }
    [[nodiscard]] VkImageView view() const noexcept { return view_; }
    [[nodiscard]] VkFormat format() const noexcept { return format_; }

private:
    VkDevice device_;
    VmaAllocator allocator_;
    VkImage image_{VK_NULL_HANDLE};
    VmaAllocation allocation_{VK_NULL_HANDLE};
    VkImageView view_{VK_NULL_HANDLE};
    VkFormat format_;
};

}
