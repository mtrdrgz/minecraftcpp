#pragma once
#include "../IRenderDevice.h"
#include <volk.h>
#include <vk_mem_alloc.h>

namespace mc::render {

class TextureVK : public ITexture {
public:
    TextureVK(VkDevice device, VmaAllocator allocator, const TextureDesc& desc);
    ~TextureVK() override;

    VkImage getHandle() const { return m_image; }
    VkImageView getView() const { return m_imageView; }
    VkSampler getSampler() const { return m_sampler; }
    const TextureDesc& getDesc() const { return m_desc; }

private:
    void createSampler();
    void createImageView();

    VkDevice      m_device;
    VmaAllocator  m_allocator;
    TextureDesc   m_desc;
    
    VkImage       m_image      = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkImageView   m_imageView  = VK_NULL_HANDLE;
    VkSampler     m_sampler    = VK_NULL_HANDLE;
};

} // namespace mc::render
