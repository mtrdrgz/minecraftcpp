#include "SwapchainVK.h"
#include <algorithm>

namespace mc::render {

SwapchainVK::SwapchainVK(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t graphicsFamily, uint32_t presentFamily)
    : m_device(device), m_physicalDevice(physicalDevice), m_surface(surface), m_graphicsFamily(graphicsFamily), m_presentFamily(presentFamily) {
}

SwapchainVK::~SwapchainVK() {
    cleanup();
}

void SwapchainVK::recreate(uint32_t width, uint32_t height, bool vsync) {
    cleanup();

    // 1. Surface capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &capabilities));

    // 2. Surface formats
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, formats.data());

    VkSurfaceFormatKHR selectedFormat = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            selectedFormat = f;
            break;
        }
    }
    m_format = selectedFormat.format;

    // 3. Present modes
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, presentModes.data());

    VkPresentModeKHR selectedMode = VK_PRESENT_MODE_FIFO_KHR; // Guaranteed to be available
    if (!vsync) {
        for (const auto& mode : presentModes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                selectedMode = mode;
                break;
            }
        }
        if (selectedMode == VK_PRESENT_MODE_FIFO_KHR) {
            for (const auto& mode : presentModes) {
                if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                    selectedMode = mode;
                    break;
                }
            }
        }
    }

    // 4. Extent
    if (capabilities.currentExtent.width != 0xFFFFFFFF) {
        m_extent = capabilities.currentExtent;
    } else {
        m_extent.width = std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        m_extent.height = std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }

    // 5. Image count
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    // 6. Create Swapchain
    VkSwapchainCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    createInfo.surface          = m_surface;
    createInfo.minImageCount    = imageCount;
    createInfo.imageFormat      = selectedFormat.format;
    createInfo.imageColorSpace  = selectedFormat.colorSpace;
    createInfo.imageExtent      = m_extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = { m_graphicsFamily, m_presentFamily };
    if (m_graphicsFamily != m_presentFamily) {
        createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices   = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform   = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode     = selectedMode;
    createInfo.clipped         = VK_TRUE;
    createInfo.oldSwapchain    = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain));

    // 7. Get Images
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr);
    m_images.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_images.data());

    // 8. Create Image Views
    m_imageViews.resize(imageCount);
    for (size_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        viewInfo.image                           = m_images[i];
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = m_format;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        VK_CHECK(vkCreateImageView(m_device, &viewInfo, nullptr, &m_imageViews[i]));
    }
}

uint32_t SwapchainVK::acquireNextImage(VkSemaphore signalSemaphore) {
    VkResult result = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, signalSemaphore, VK_NULL_HANDLE, &m_currentImageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return 0xFFFFFFFF; // Signal recreation needed
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        VK_CHECK(result);
    }
    return m_currentImageIndex;
}

void SwapchainVK::present(VkQueue queue, VkSemaphore waitSemaphore) {
    VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &waitSemaphore;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &m_swapchain;
    presentInfo.pImageIndices      = &m_currentImageIndex;

    VkResult result = vkQueuePresentKHR(queue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        // Handled by next recreate call
    } else {
        VK_CHECK(result);
    }
}

void SwapchainVK::cleanup() {
    for (auto view : m_imageViews) {
        vkDestroyImageView(m_device, view, nullptr);
    }
    m_imageViews.clear();

    if (m_swapchain) {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

} // namespace mc::render
