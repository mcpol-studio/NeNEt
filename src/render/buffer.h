#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <cstddef>

namespace nenet {

class Buffer {
public:
    Buffer(VmaAllocator allocator,
           VkDeviceSize size,
           VkBufferUsageFlags usage,
           VmaMemoryUsage memUsage,
           VmaAllocationCreateFlags flags = 0);
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    void upload(const void* src, VkDeviceSize size, VkDeviceSize offset = 0);

    [[nodiscard]] VkBuffer handle() const noexcept { return buffer_; }
    [[nodiscard]] VkDeviceSize size() const noexcept { return size_; }
    [[nodiscard]] void* mapped() const noexcept { return info_.pMappedData; }

private:
    VmaAllocator allocator_;
    VkBuffer buffer_{VK_NULL_HANDLE};
    VmaAllocation allocation_{VK_NULL_HANDLE};
    VmaAllocationInfo info_{};
    VkDeviceSize size_;
};

}
