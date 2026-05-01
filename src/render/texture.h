#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <filesystem>

namespace nenet {

class VulkanContext;

class Texture {
public:
    enum class FilterMode { Nearest, Linear };
    enum class AddressMode { Repeat, ClampToEdge };

    Texture(VulkanContext& ctx, const std::filesystem::path& path,
            FilterMode filter = FilterMode::Nearest,
            AddressMode address = AddressMode::Repeat);

    Texture(VulkanContext& ctx, const uint8_t* rgbaPixels, uint32_t width, uint32_t height,
            FilterMode filter = FilterMode::Linear,
            AddressMode address = AddressMode::ClampToEdge);

    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    [[nodiscard]] VkImage image() const noexcept { return image_; }
    [[nodiscard]] VkImageView view() const noexcept { return view_; }
    [[nodiscard]] VkSampler sampler() const noexcept { return sampler_; }
    [[nodiscard]] uint32_t width() const noexcept { return width_; }
    [[nodiscard]] uint32_t height() const noexcept { return height_; }

private:
    VulkanContext& ctx_;
    VkImage image_{VK_NULL_HANDLE};
    VmaAllocation allocation_{VK_NULL_HANDLE};
    VkImageView view_{VK_NULL_HANDLE};
    VkSampler sampler_{VK_NULL_HANDLE};
    uint32_t width_{0};
    uint32_t height_{0};
};

}
