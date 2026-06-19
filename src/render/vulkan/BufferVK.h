#pragma once
#include "../IRenderDevice.h"
#include <volk.h>
#include <vk_mem_alloc.h>

namespace mc::render {

class BufferVK : public IBuffer {
public:
    BufferVK(VmaAllocator allocator, const BufferDesc& desc);
    ~BufferVK() override;

    /**
     * Uploads data to the buffer.
     * For dynamic buffers, this uses the persistent mapping.
     * For non-dynamic buffers, this will attempt to map the memory (if host-visible)
     * or should be handled by the command list via a staging buffer.
     */
    void upload(const void* data, size_t size, size_t offset = 0);

    VkBuffer getHandle() const { return m_buffer; }
    const BufferDesc& getDesc() const { return m_desc; }
    void* getMappedData() const { return m_allocationInfo.pMappedData; }

private:
    VmaAllocator      m_allocator;
    BufferDesc        m_desc;
    VkBuffer          m_buffer     = VK_NULL_HANDLE;
    VmaAllocation     m_allocation = VK_NULL_HANDLE;
    VmaAllocationInfo m_allocationInfo{};
};

} // namespace mc::render
