#include "renderer.h"

#include "camera.h"
#include "chunk_mesh.h"
#include "depth_image.h"
#include "pipeline.h"
#include "swapchain.h"
#include "vertex.h"
#include "vulkan_context.h"

#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include <stdexcept>
#include <string>

namespace nenet {

namespace {

void check(VkResult r, const char* what) {
    if (r != VK_SUCCESS) {
        throw std::runtime_error(std::string("Vulkan 调用失败: ") + what + " code=" + std::to_string(r));
    }
}

}

Renderer::Renderer(VulkanContext& ctx, Swapchain& swapchain, Camera& camera, ChunkMesh& mesh)
    : ctx_(ctx), swapchain_(swapchain), camera_(camera), mesh_(mesh) {
    createFrames();
    createPerImageSemaphores();
    depth_ = std::make_unique<DepthImage>(ctx_.device(), ctx_.allocator(),
                                          swapchain_.extent().width,
                                          swapchain_.extent().height);
    pipeline_ = std::make_unique<CubePipeline>(ctx_.device(), swapchain_.imageFormat(),
                                                depth_->format());
    spdlog::info("Renderer 已初始化 indexCount={}", mesh_.indexCount());
}

Renderer::~Renderer() {
    vkDeviceWaitIdle(ctx_.device());
    pipeline_.reset();
    depth_.reset();
    destroyPerImageSemaphores();
    destroyFrames();
    spdlog::info("Renderer 已销毁");
}

void Renderer::createFrames() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = ctx_.graphicsQueueFamily();

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (auto& f : frames_) {
        check(vkCreateCommandPool(ctx_.device(), &poolInfo, nullptr, &f.commandPool),
              "vkCreateCommandPool");
        VkCommandBufferAllocateInfo cbInfo{};
        cbInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cbInfo.commandPool = f.commandPool;
        cbInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbInfo.commandBufferCount = 1;
        check(vkAllocateCommandBuffers(ctx_.device(), &cbInfo, &f.commandBuffer),
              "vkAllocateCommandBuffers");
        check(vkCreateSemaphore(ctx_.device(), &semInfo, nullptr, &f.imageAvailable),
              "vkCreateSemaphore imageAvailable");
        check(vkCreateFence(ctx_.device(), &fenceInfo, nullptr, &f.inFlight),
              "vkCreateFence inFlight");
    }
}

void Renderer::destroyFrames() {
    for (auto& f : frames_) {
        if (f.inFlight) vkDestroyFence(ctx_.device(), f.inFlight, nullptr);
        if (f.imageAvailable) vkDestroySemaphore(ctx_.device(), f.imageAvailable, nullptr);
        if (f.commandPool) vkDestroyCommandPool(ctx_.device(), f.commandPool, nullptr);
        f = {};
    }
}

void Renderer::createPerImageSemaphores() {
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    renderFinishedPerImage_.resize(swapchain_.imageCount(), VK_NULL_HANDLE);
    for (auto& s : renderFinishedPerImage_) {
        check(vkCreateSemaphore(ctx_.device(), &semInfo, nullptr, &s),
              "vkCreateSemaphore renderFinished");
    }
}

void Renderer::destroyPerImageSemaphores() {
    for (auto& s : renderFinishedPerImage_) {
        if (s) vkDestroySemaphore(ctx_.device(), s, nullptr);
    }
    renderFinishedPerImage_.clear();
}

void Renderer::transitionColor(VkCommandBuffer cmd, VkImage image,
                               VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    VkDependencyInfo dep{};
    dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(cmd, &dep);
}

void Renderer::transitionDepth(VkCommandBuffer cmd, VkImage image) {
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                           VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    VkDependencyInfo dep{};
    dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(cmd, &dep);
}

void Renderer::recordCommand(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(vkBeginCommandBuffer(cmd, &begin), "vkBeginCommandBuffer");

    transitionColor(cmd, swapchain_.image(imageIndex),
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    transitionDepth(cmd, depth_->image());

    VkRenderingAttachmentInfo colorAttach{};
    colorAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttach.imageView = swapchain_.imageView(imageIndex);
    colorAttach.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttach.clearValue.color = {{0.50f, 0.70f, 0.90f, 1.0f}};

    VkRenderingAttachmentInfo depthAttach{};
    depthAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttach.imageView = depth_->view();
    depthAttach.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttach.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttach.clearValue.depthStencil = {1.0f, 0};

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.renderArea.extent = swapchain_.extent();
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttach;
    renderInfo.pDepthAttachment = &depthAttach;

    vkCmdBeginRendering(cmd, &renderInfo);

    VkViewport vp{};
    vp.x = 0.0f;
    vp.y = 0.0f;
    vp.width = static_cast<float>(swapchain_.extent().width);
    vp.height = static_cast<float>(swapchain_.extent().height);
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchain_.extent();
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_->handle());

    PushConstants pc{};
    pc.mvp = camera_.viewProj();
    vkCmdPushConstants(cmd, pipeline_->layout(), VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(PushConstants), &pc);

    mesh_.draw(cmd);

    vkCmdEndRendering(cmd);

    transitionColor(cmd, swapchain_.image(imageIndex),
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    check(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");
}

void Renderer::drawFrame() {
    auto& f = frames_[currentFrame_];

    check(vkWaitForFences(ctx_.device(), 1, &f.inFlight, VK_TRUE, UINT64_MAX),
          "vkWaitForFences");

    uint32_t imageIndex = 0;
    check(vkAcquireNextImageKHR(ctx_.device(), swapchain_.handle(), UINT64_MAX,
                                f.imageAvailable, VK_NULL_HANDLE, &imageIndex),
          "vkAcquireNextImageKHR");

    check(vkResetFences(ctx_.device(), 1, &f.inFlight), "vkResetFences");
    check(vkResetCommandBuffer(f.commandBuffer, 0), "vkResetCommandBuffer");

    recordCommand(f.commandBuffer, imageIndex);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &f.imageAvailable;
    submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &f.commandBuffer;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &renderFinishedPerImage_[imageIndex];
    check(vkQueueSubmit(ctx_.graphicsQueue(), 1, &submit, f.inFlight), "vkQueueSubmit");

    VkSwapchainKHR sc = swapchain_.handle();
    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &renderFinishedPerImage_[imageIndex];
    present.swapchainCount = 1;
    present.pSwapchains = &sc;
    present.pImageIndices = &imageIndex;
    check(vkQueuePresentKHR(ctx_.graphicsQueue(), &present), "vkQueuePresentKHR");

    currentFrame_ = (currentFrame_ + 1) % kFramesInFlight;
}

}
