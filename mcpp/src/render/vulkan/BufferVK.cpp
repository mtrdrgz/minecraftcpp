#include "BufferVK.h"
#include "VulkanUtils.h"
#include <cstring>

namespace mc::render {

BufferVK::BufferVK(VmaAllocator allocator, const BufferDesc& desc)
    : m_allocator(allocator), m_desc(desc) {

    VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = desc.size;
    
    switch (desc.usage) {
        case BufferUsage::Vertex:  bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;  break;
        case BufferUsage::Index:   bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;   break;
        case BufferUsage::Uniform: bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; break;
        case BufferUsage::Staging: bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT; break;
    }

    // Allow all buffers to be transfer targets for potential staging uploads
    bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    
    if (desc.dynamic || desc.usage == BufferUsage::Staging) {
        // CPU-visible, host-coherent, and persistently mapped for dynamic buffers
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        if (desc.dynamic) {
            allocCreateInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }
    }

    VK_CHECK(vmaCreateBuffer(m_allocator, &bufferInfo, &allocCreateInfo, &m_buffer, &m_allocation, &m_allocationInfo));
}

BufferVK::~BufferVK() {
    if (m_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
    }
}

void BufferVK::upload(const void* data, size_t size, size_t offset) {
    if (size == 0) return;
    
    if (offset + size > m_desc.size) {
        MC_LOG_ERROR("BufferVK::upload: Attempted to upload {} bytes at offset {} to a buffer of size {}", size, offset, m_desc.size);
        return;
    }

    if (m_allocationInfo.pMappedData) {
        // Persistently mapped
        std::memcpy(static_cast<uint8_t*>(m_allocationInfo.pMappedData) + offset, data, size);
    } else {
        // Not persistently mapped - try temporary mapping
        // This will only work if the buffer was created with host access flags
        void* mappedData = nullptr;
        VK_CHECK(vmaMapMemory(m_allocator, m_allocation, &mappedData));
        std::memcpy(static_cast<uint8_t*>(mappedData) + offset, data, size);
        vmaUnmapMemory(m_allocator, m_allocation);
    }
}

} // namespace mc::render
