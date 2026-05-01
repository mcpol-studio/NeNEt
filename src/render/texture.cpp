#include "texture.h"

#include "buffer.h"
#include "vulkan_context.h"

#include <stb_image.h>
#include <spdlog/spdlog.h>

#include <stdexcept>

namespace nenet {

namespace {

void transition(VkCommandBuffer cmd, VkImage image,
                VkImageLayout oldLayout, VkImageLayout newLayout,
                VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
                VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess) {
    VkImageMemoryBarrier2 b{};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    b.srcStageMask = srcStage;
    b.srcAccessMask = srcAccess;
    b.dstStageMask = dstStage;
    b.dstAccessMask = dstAccess;
    b.oldLayout = oldLayout;
    b.newLayout = newLayout;
    b.image = image;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    b.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    VkDependencyInfo d{};
    d.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    d.imageMemoryBarrierCount = 1;
    d.pImageMemoryBarriers = &b;
    vkCmdPipelineBarrier2(cmd, &d);
}

}

Texture::Texture(VulkanContext& ctx, const std::filesystem::path& path,
                 FilterMode filter, AddressMode address) : ctx_(ctx) {
    int w, h, channels;
    stbi_uc* pixels = stbi_load(path.string().c_str(), &w, &h, &channels, STBI_rgb_alpha);
    if (pixels == nullptr) {
        throw std::runtime_error("纹理加载失败: " + path.string() + " - " + stbi_failure_reason());
    }
    width_ = static_cast<uint32_t>(w);
    height_ = static_cast<uint32_t>(h);
    const VkDeviceSize size = static_cast<VkDeviceSize>(w) * h * 4;

    Buffer staging(ctx_.allocator(), size,
                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VMA_MEMORY_USAGE_AUTO,
                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                   VMA_ALLOCATION_CREATE_MAPPED_BIT);
    staging.upload(pixels, size);
    stbi_image_free(pixels);

    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgInfo.extent = {width_, height_, 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    if (vmaCreateImage(ctx_.allocator(), &imgInfo, &allocInfo, &image_, &allocation_, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("vmaCreateImage 失败 (texture)");
    }

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = ctx_.graphicsQueueFamily();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    VkCommandPool pool;
    if (vkCreateCommandPool(ctx_.device(), &poolInfo, nullptr, &pool) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateCommandPool 失败 (texture upload)");
    }

    VkCommandBufferAllocateInfo cbInfo{};
    cbInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbInfo.commandPool = pool;
    cbInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbInfo.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(ctx_.device(), &cbInfo, &cmd);

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);

    transition(cmd, image_,
               VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
               VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0,
               VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {width_, height_, 1};
    vkCmdCopyBufferToImage(cmd, staging.handle(), image_,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    transition(cmd, image_,
               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
               VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
               VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(ctx_.graphicsQueue(), 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx_.graphicsQueue());

    vkDestroyCommandPool(ctx_.device(), pool, nullptr);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(ctx_.device(), &viewInfo, nullptr, &view_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateImageView 失败 (texture)");
    }

    const VkFilter vfilter = (filter == FilterMode::Linear) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    const VkSamplerAddressMode vaddr = (address == AddressMode::ClampToEdge)
        ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
        : VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerCreateInfo sampInfo{};
    sampInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampInfo.magFilter = vfilter;
    sampInfo.minFilter = vfilter;
    sampInfo.mipmapMode = (filter == FilterMode::Linear)
        ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampInfo.addressModeU = vaddr;
    sampInfo.addressModeV = vaddr;
    sampInfo.addressModeW = vaddr;
    sampInfo.maxLod = 1.0f;
    if (vkCreateSampler(ctx_.device(), &sampInfo, nullptr, &sampler_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateSampler 失败");
    }

    spdlog::info("Texture 已加载 {} ({}x{})", path.filename().string(), width_, height_);
}

Texture::Texture(VulkanContext& ctx, const uint8_t* rgbaPixels, uint32_t w, uint32_t h,
                 FilterMode filter, AddressMode address) : ctx_(ctx) {
    if (rgbaPixels == nullptr || w == 0 || h == 0) {
        throw std::runtime_error("Texture: 内存像素无效");
    }
    width_ = w;
    height_ = h;
    const VkDeviceSize size = static_cast<VkDeviceSize>(w) * h * 4;

    Buffer staging(ctx_.allocator(), size,
                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VMA_MEMORY_USAGE_AUTO,
                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                   VMA_ALLOCATION_CREATE_MAPPED_BIT);
    staging.upload(rgbaPixels, size);

    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgInfo.extent = {width_, height_, 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    if (vmaCreateImage(ctx_.allocator(), &imgInfo, &allocInfo, &image_, &allocation_, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("vmaCreateImage 失败 (texture-mem)");
    }

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = ctx_.graphicsQueueFamily();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    VkCommandPool pool;
    if (vkCreateCommandPool(ctx_.device(), &poolInfo, nullptr, &pool) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateCommandPool 失败 (texture-mem)");
    }

    VkCommandBufferAllocateInfo cbInfo{};
    cbInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbInfo.commandPool = pool;
    cbInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbInfo.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(ctx_.device(), &cbInfo, &cmd);

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);

    transition(cmd, image_,
               VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
               VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0,
               VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {width_, height_, 1};
    vkCmdCopyBufferToImage(cmd, staging.handle(), image_,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    transition(cmd, image_,
               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
               VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
               VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);

    vkEndCommandBuffer(cmd);
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(ctx_.graphicsQueue(), 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx_.graphicsQueue());
    vkDestroyCommandPool(ctx_.device(), pool, nullptr);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(ctx_.device(), &viewInfo, nullptr, &view_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateImageView 失败 (texture-mem)");
    }

    const VkFilter vfilter = (filter == FilterMode::Linear) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    const VkSamplerAddressMode vaddr = (address == AddressMode::ClampToEdge)
        ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
        : VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerCreateInfo sampInfo{};
    sampInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampInfo.magFilter = vfilter;
    sampInfo.minFilter = vfilter;
    sampInfo.mipmapMode = (filter == FilterMode::Linear)
        ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampInfo.addressModeU = vaddr;
    sampInfo.addressModeV = vaddr;
    sampInfo.addressModeW = vaddr;
    sampInfo.maxLod = 1.0f;
    if (vkCreateSampler(ctx_.device(), &sampInfo, nullptr, &sampler_) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateSampler 失败 (mem)");
    }

    spdlog::info("Texture (mem) 已上传 ({}x{})", width_, height_);
}

Texture::~Texture() {
    if (sampler_) vkDestroySampler(ctx_.device(), sampler_, nullptr);
    if (view_) vkDestroyImageView(ctx_.device(), view_, nullptr);
    if (image_) vmaDestroyImage(ctx_.allocator(), image_, allocation_);
}

}
