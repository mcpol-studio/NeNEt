#include "buffer.h"

#include <cstring>
#include <stdexcept>

namespace nenet {

Buffer::Buffer(VmaAllocator allocator,
               VkDeviceSize size,
               VkBufferUsageFlags usage,
               VmaMemoryUsage memUsage,
               VmaAllocationCreateFlags flags)
    : allocator_(allocator), size_(size) {
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = size;
    bufInfo.usage = usage;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memUsage;
    allocInfo.flags = flags;

    if (vmaCreateBuffer(allocator_, &bufInfo, &allocInfo, &buffer_, &allocation_, &info_) != VK_SUCCESS) {
        throw std::runtime_error("vmaCreateBuffer 失败");
    }
}

Buffer::~Buffer() {
    if (buffer_ != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator_, buffer_, allocation_);
    }
}

void Buffer::upload(const void* src, VkDeviceSize size, VkDeviceSize offset) {
    if (info_.pMappedData != nullptr) {
        std::memcpy(static_cast<char*>(info_.pMappedData) + offset, src, static_cast<size_t>(size));
        vmaFlushAllocation(allocator_, allocation_, offset, size);
    } else {
        void* mapped = nullptr;
        if (vmaMapMemory(allocator_, allocation_, &mapped) != VK_SUCCESS) {
            throw std::runtime_error("vmaMapMemory 失败");
        }
        std::memcpy(static_cast<char*>(mapped) + offset, src, static_cast<size_t>(size));
        vmaUnmapMemory(allocator_, allocation_);
        vmaFlushAllocation(allocator_, allocation_, offset, size);
    }
}

}
