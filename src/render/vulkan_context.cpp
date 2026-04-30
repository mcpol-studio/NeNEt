#include "vulkan_context.h"

#include "../core/window.h"

#include <VkBootstrap.h>
#include <spdlog/spdlog.h>

#include <stdexcept>

namespace nenet {

VulkanContext::VulkanContext(const Window& window) {
    vkb::InstanceBuilder builder;
    auto instRet = builder
        .set_app_name("NeNEt")
        .set_engine_name("NeNEt-Engine")
        .require_api_version(1, 3, 0)
        .request_validation_layers(true)
        .use_default_debug_messenger()
        .build();
    if (!instRet) {
        throw std::runtime_error("Vulkan instance 创建失败: " + instRet.error().message());
    }
    vkb::Instance vkbInst = instRet.value();
    instance_ = vkbInst.instance;
    debugMessenger_ = vkbInst.debug_messenger;
    spdlog::info("Vulkan instance 已创建 (api 1.3)");

    surface_ = window.createSurface(instance_);
    spdlog::info("窗口 surface 已创建");

    VkPhysicalDeviceVulkan13Features feat13{};
    feat13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    feat13.dynamicRendering = VK_TRUE;
    feat13.synchronization2 = VK_TRUE;

    VkPhysicalDeviceVulkan12Features feat12{};
    feat12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    feat12.bufferDeviceAddress = VK_TRUE;
    feat12.descriptorIndexing = VK_TRUE;

    vkb::PhysicalDeviceSelector selector{vkbInst};
    auto physRet = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(feat13)
        .set_required_features_12(feat12)
        .set_surface(surface_)
        .select();
    if (!physRet) {
        throw std::runtime_error("找不到合适的 GPU: " + physRet.error().message());
    }
    vkb::PhysicalDevice vkbPhys = physRet.value();
    physicalDevice_ = vkbPhys.physical_device;
    spdlog::info("已选择 GPU: {}", vkbPhys.name);

    vkb::DeviceBuilder devBuilder{vkbPhys};
    auto devRet = devBuilder.build();
    if (!devRet) {
        throw std::runtime_error("逻辑设备创建失败: " + devRet.error().message());
    }
    vkb::Device vkbDev = devRet.value();
    device_ = vkbDev.device;

    auto qRet = vkbDev.get_queue(vkb::QueueType::graphics);
    if (!qRet) {
        throw std::runtime_error("获取 graphics queue 失败: " + qRet.error().message());
    }
    graphicsQueue_ = qRet.value();
    graphicsQueueFamily_ = vkbDev.get_queue_index(vkb::QueueType::graphics).value();
    spdlog::info("逻辑设备已创建 graphicsQueueFamily={}", graphicsQueueFamily_);

    VmaAllocatorCreateInfo allocInfo{};
    allocInfo.physicalDevice = physicalDevice_;
    allocInfo.device = device_;
    allocInfo.instance = instance_;
    allocInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    if (vmaCreateAllocator(&allocInfo, &allocator_) != VK_SUCCESS) {
        throw std::runtime_error("VMA allocator 创建失败");
    }
    spdlog::info("VMA allocator 已创建");
}

VulkanContext::~VulkanContext() {
    if (allocator_ != VK_NULL_HANDLE) {
        vmaDestroyAllocator(allocator_);
    }
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        vkDestroyDevice(device_, nullptr);
    }
    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
    }
    if (debugMessenger_ != VK_NULL_HANDLE) {
        vkb::destroy_debug_utils_messenger(instance_, debugMessenger_);
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
    }
    spdlog::info("Vulkan 上下文已销毁");
}

void VulkanContext::waitIdle() const {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }
}

}
