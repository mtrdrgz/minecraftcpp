#include "TextureVK.h"
#include "VulkanUtils.h"

namespace mc::render {

static VkFormat toVkFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::RGBA8: return VK_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::RGB8:  return VK_FORMAT_R8G8B8_UNORM;
        case TextureFormat::R8:    return VK_FORMAT_R8_UNORM;
        case TextureFormat::Depth24Stencil8: return VK_FORMAT_D24_UNORM_S8_UINT;
        default: return VK_FORMAT_UNDEFINED;
    }
}

TextureVK::TextureVK(VkDevice device, VmaAllocator allocator, const TextureDesc& desc)
    : m_device(device), m_allocator(allocator), m_desc(desc) {

    VkFormat format = toVkFormat(desc.format);

    VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = desc.width;
    imageInfo.extent.height = desc.height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = desc.mipLevels;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = format;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (desc.format == TextureFormat::Depth24Stencil8) {
        imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    } else if (desc.genMipmaps) {
        imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    VK_CHECK(vmaCreateImage(m_allocator, &imageInfo, &allocInfo, &m_image, &m_allocation, nullptr));

    createImageView();
    createSampler();
}

TextureVK::~TextureVK() {
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_sampler, nullptr);
    }
    if (m_imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_imageView, nullptr);
    }
    if (m_image != VK_NULL_HANDLE) {
        vmaDestroyImage(m_allocator, m_image, m_allocation);
    }
}

void TextureVK::createImageView() {
    VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewInfo.image                           = m_image;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = toVkFormat(m_desc.format);
    viewInfo.subresourceRange.aspectMask     = (m_desc.format == TextureFormat::Depth24Stencil8) 
                                               ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT) 
                                               : VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = m_desc.mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    VK_CHECK(vkCreateImageView(m_device, &viewInfo, nullptr, &m_imageView));
}

void TextureVK::createSampler() {
    VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    
    switch (m_desc.filter) {
        case FilterMode::Nearest:
            samplerInfo.magFilter = VK_FILTER_NEAREST;
            samplerInfo.minFilter = VK_FILTER_NEAREST;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;
        case FilterMode::Linear:
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;
        case FilterMode::LinearMipLinear:
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            break;
    }

    VkSamplerAddressMode addressMode = (m_desc.wrap == WrapMode::Repeat) 
                                       ? VK_SAMPLER_ADDRESS_MODE_REPEAT 
                                       : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeU = addressMode;
    samplerInfo.addressModeV = addressMode;
    samplerInfo.addressModeW = addressMode;
    
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy    = 1.0f;
    samplerInfo.borderColor      = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable    = VK_FALSE;
    samplerInfo.compareOp        = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipLodBias       = 0.0f;
    samplerInfo.minLod           = 0.0f;
    samplerInfo.maxLod           = static_cast<float>(m_desc.mipLevels);

    VK_CHECK(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_sampler));
}

} // namespace mc::render
