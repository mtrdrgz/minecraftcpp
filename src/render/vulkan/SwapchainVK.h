#pragma once
#include "VulkanUtils.h"
#include <vector>

namespace mc::render {

class SwapchainVK {
public:
    SwapchainVK(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t graphicsFamily, uint32_t presentFamily);
    ~SwapchainVK();

    void recreate(uint32_t width, uint32_t height, bool vsync);
    
    uint32_t acquireNextImage(VkSemaphore signalSemaphore);
    void present(VkQueue queue, VkSemaphore waitSemaphore);

    VkFormat getFormat() const { return m_format; }
    VkExtent2D getExtent() const { return m_extent; }
    uint32_t getImageCount() const { return static_cast<uint32_t>(m_images.size()); }
    VkImageView getImageView(uint32_t index) const { return m_imageViews[index]; }
    VkImage getImage(uint32_t index) const { return m_images[index]; }
    uint32_t getCurrentImageIndex() const { return m_currentImageIndex; }

private:
    void cleanup();

    VkDevice m_device;
    VkPhysicalDevice m_physicalDevice;
    VkSurfaceKHR m_surface;
    uint32_t m_graphicsFamily;
    uint32_t m_presentFamily;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_format;
    VkExtent2D m_extent;
    
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
    uint32_t m_currentImageIndex = 0;
};

} // namespace mc::render
