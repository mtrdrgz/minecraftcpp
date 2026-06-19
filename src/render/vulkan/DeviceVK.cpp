#include "DeviceVK.h"
#include "VulkanUtils.h"
#include "SwapchainVK.h"
#include "BufferVK.h"
#include "TextureVK.h"
#include "PipelineVK.h"
#include "CommandListVK.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <vector>
#include <set>
#include <algorithm>

namespace mc::render {

struct DeviceVK::Impl {
    VkInstance       instance       = VK_NULL_HANDLE;
    VkSurfaceKHR     surface        = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice         device         = VK_NULL_HANDLE;
    VkQueue          graphicsQueue  = VK_NULL_HANDLE;
    VkQueue          presentQueue   = VK_NULL_HANDLE;
    VmaAllocator     allocator      = VK_NULL_HANDLE;
    std::unique_ptr<SwapchainVK> swapchain;

    uint32_t graphicsFamily = 0xFFFFFFFF;
    uint32_t presentFamily  = 0xFFFFFFFF;
    bool     vsync          = true;

    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    uint32_t currentFrame = 0;
    uint32_t imageIndex   = 0;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence>     inFlightFences;
    
    std::vector<VkCommandPool>   commandPools;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<std::unique_ptr<CommandListVK>> commandLists;

    ~Impl() {
        if (device) {
            vkDeviceWaitIdle(device);
            for (auto s : imageAvailableSemaphores) if (s) vkDestroySemaphore(device, s, nullptr);
            for (auto s : renderFinishedSemaphores) if (s) vkDestroySemaphore(device, s, nullptr);
            for (auto f : inFlightFences) if (f) vkDestroyFence(device, f, nullptr);
            for (auto p : commandPools) if (p) vkDestroyCommandPool(device, p, nullptr);

            if (allocator) vmaDestroyAllocator(allocator);
            vkDestroyDevice(device, nullptr);
        }
        if (surface) {
            vkDestroySurfaceKHR(instance, surface, nullptr);
        }
        if (instance) {
            vkDestroyInstance(instance, nullptr);
        }
    }
};

std::unique_ptr<DeviceVK> DeviceVK::create(HWND hwnd) {
    auto device = std::unique_ptr<DeviceVK>(new DeviceVK());
    if (!device->init(hwnd)) {
        return nullptr;
    }
    return device;
}

DeviceVK::~DeviceVK() = default;

bool DeviceVK::init(HWND hwnd) {
    m_impl = std::make_unique<Impl>();

    if (volkInitialize() != VK_SUCCESS) {
        MC_LOG_ERROR("Failed to initialize volk");
        return false;
    }

    // 1. Instance Creation
    VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    appInfo.pApplicationName   = "mcpp";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "mcpp";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    };

    std::vector<const char*> layers;
#ifdef _DEBUG
    layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

    VkInstanceCreateInfo createInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount       = static_cast<uint32_t>(layers.size());
    createInfo.ppEnabledLayerNames     = layers.data();

    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_impl->instance));
    volkLoadInstance(m_impl->instance);

    // 2. Win32 Surface Creation
    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{ VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
    surfaceCreateInfo.hwnd      = hwnd;
    surfaceCreateInfo.hinstance = GetModuleHandle(nullptr);
    VK_CHECK(vkCreateWin32SurfaceKHR(m_impl->instance, &surfaceCreateInfo, nullptr, &m_impl->surface));

    // 3. Physical Device Selection
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_impl->instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        MC_LOG_ERROR("No Vulkan physical devices found");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_impl->instance, &deviceCount, devices.data());

    for (const auto& dev : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            m_impl->physicalDevice = dev;
            break;
        }
    }
    if (m_impl->physicalDevice == VK_NULL_HANDLE) {
        m_impl->physicalDevice = devices[0];
    }

    // 4. Queue Family Selection
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_impl->physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_impl->physicalDevice, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            m_impl->graphicsFamily = i;
        }
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_impl->physicalDevice, i, m_impl->surface, &presentSupport);
        if (presentSupport) {
            m_impl->presentFamily = i;
        }
        if (m_impl->graphicsFamily != 0xFFFFFFFF && m_impl->presentFamily != 0xFFFFFFFF) {
            break;
        }
    }

    if (m_impl->graphicsFamily == 0xFFFFFFFF || m_impl->presentFamily == 0xFFFFFFFF) {
        MC_LOG_ERROR("Failed to find suitable queue families");
        return false;
    }

    // 5. Logical Device Creation
    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { m_impl->graphicsFamily, m_impl->presentFamily };

    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount       = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    
    VkPhysicalDeviceFeatures deviceFeatures{};
    
    VkPhysicalDeviceVulkan13Features features13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    features13.dynamicRendering = VK_TRUE;

    VkDeviceCreateInfo deviceCreateInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    deviceCreateInfo.pNext                   = &features13;
    deviceCreateInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos       = queueCreateInfos.data();
    deviceCreateInfo.pEnabledFeatures        = &deviceFeatures;
    deviceCreateInfo.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VK_CHECK(vkCreateDevice(m_impl->physicalDevice, &deviceCreateInfo, nullptr, &m_impl->device));
    volkLoadDevice(m_impl->device);

    vkGetDeviceQueue(m_impl->device, m_impl->graphicsFamily, 0, &m_impl->graphicsQueue);
    vkGetDeviceQueue(m_impl->device, m_impl->presentFamily, 0, &m_impl->presentQueue);

    // 6. VMA Initialization
    VmaVulkanFunctions vmaVulkanFunctions = {};
    vmaVulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vmaVulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorInfo.physicalDevice   = m_impl->physicalDevice;
    allocatorInfo.device           = m_impl->device;
    allocatorInfo.instance         = m_impl->instance;
    allocatorInfo.pVulkanFunctions = &vmaVulkanFunctions;

    VK_CHECK(vmaCreateAllocator(&allocatorInfo, &m_impl->allocator));

    // 7. Swapchain Initialization
    m_impl->swapchain = std::make_unique<SwapchainVK>(
        m_impl->device, 
        m_impl->physicalDevice, 
        m_impl->surface, 
        m_impl->graphicsFamily, 
        m_impl->presentFamily
    );

    // 8. Synchronization and Command Management
    m_impl->imageAvailableSemaphores.resize(Impl::MAX_FRAMES_IN_FLIGHT);
    m_impl->renderFinishedSemaphores.resize(Impl::MAX_FRAMES_IN_FLIGHT);
    m_impl->inFlightFences.resize(Impl::MAX_FRAMES_IN_FLIGHT);
    m_impl->commandPools.resize(Impl::MAX_FRAMES_IN_FLIGHT);
    m_impl->commandBuffers.resize(Impl::MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo     fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_impl->graphicsFamily;

    for (int i = 0; i < Impl::MAX_FRAMES_IN_FLIGHT; i++) {
        VK_CHECK(vkCreateSemaphore(m_impl->device, &semaphoreInfo, nullptr, &m_impl->imageAvailableSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(m_impl->device, &semaphoreInfo, nullptr, &m_impl->renderFinishedSemaphores[i]));
        VK_CHECK(vkCreateFence(m_impl->device, &fenceInfo, nullptr, &m_impl->inFlightFences[i]));
        VK_CHECK(vkCreateCommandPool(m_impl->device, &poolInfo, nullptr, &m_impl->commandPools[i]));

        VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        allocInfo.commandPool        = m_impl->commandPools[i];
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(m_impl->device, &allocInfo, &m_impl->commandBuffers[i]));

        m_impl->commandLists.push_back(std::make_unique<CommandListVK>(m_impl->commandBuffers[i]));
    }

    return true;
}

IBuffer* DeviceVK::createBuffer(const BufferDesc& desc) {
    return new BufferVK(m_impl->allocator, desc);
}

ITexture* DeviceVK::createTexture(const TextureDesc& desc) {
    return new TextureVK(m_impl->device, m_impl->allocator, desc);
}

IPipeline* DeviceVK::createPipeline(const PipelineDesc& desc) {
    return new PipelineVK(m_impl->device, desc);
}

void DeviceVK::destroyBuffer(IBuffer* buffer) {
    delete static_cast<BufferVK*>(buffer);
}

void DeviceVK::destroyTexture(ITexture* texture) {
    delete static_cast<TextureVK*>(texture);
}

void DeviceVK::destroyPipeline(IPipeline* pipeline) {
    delete static_cast<PipelineVK*>(pipeline);
}

void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.oldLayout           = oldLayout;
    barrier.newLayout           = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = 0;
        srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    } else {
        barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

ICommandList* DeviceVK::beginFrame(int32_t viewW, int32_t viewH) {
    if (!m_impl->swapchain) return nullptr;

    auto extent = m_impl->swapchain->getExtent();
    if (extent.width != static_cast<uint32_t>(viewW) || extent.height != static_cast<uint32_t>(viewH)) {
        vkDeviceWaitIdle(m_impl->device);
        m_impl->swapchain->recreate(viewW, viewH, m_impl->vsync);
        extent = m_impl->swapchain->getExtent();
    }

    VK_CHECK(vkWaitForFences(m_impl->device, 1, &m_impl->inFlightFences[m_impl->currentFrame], VK_TRUE, UINT64_MAX));

    m_impl->imageIndex = m_impl->swapchain->acquireNextImage(m_impl->imageAvailableSemaphores[m_impl->currentFrame]);
    if (m_impl->imageIndex == 0xFFFFFFFF) {
        m_impl->swapchain->recreate(viewW, viewH, m_impl->vsync);
        return nullptr;
    }

    VK_CHECK(vkResetFences(m_impl->device, 1, &m_impl->inFlightFences[m_impl->currentFrame]));

    VkCommandBuffer cmd = m_impl->commandBuffers[m_impl->currentFrame];
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    VkImage swapImage = m_impl->swapchain->getImage(m_impl->imageIndex);
    
    transitionImageLayout(cmd, swapImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingAttachmentInfo colorAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    colorAttachment.imageView   = m_impl->swapchain->getImageView(m_impl->imageIndex);
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = {{ 0.1f, 0.1f, 0.1f, 1.0f }};

    VkRenderingInfo renderingInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
    renderingInfo.renderArea.extent = extent;
    renderingInfo.layerCount        = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments    = &colorAttachment;

    vkCmdBeginRendering(cmd, &renderingInfo);

    return m_impl->commandLists[m_impl->currentFrame].get();
}

void          DeviceVK::endFrame() {
    VkCommandBuffer cmd = m_impl->commandBuffers[m_impl->currentFrame];
    vkCmdEndRendering(cmd);

    VkImage swapImage = m_impl->swapchain->getImage(m_impl->imageIndex);
    transitionImageLayout(cmd, swapImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    VkSemaphore waitSemaphores[] = { m_impl->imageAvailableSemaphores[m_impl->currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores    = waitSemaphores;
    submitInfo.pWaitDstStageMask  = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;

    VkSemaphore signalSemaphores[] = { m_impl->renderFinishedSemaphores[m_impl->currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = signalSemaphores;

    VK_CHECK(vkQueueSubmit(m_impl->graphicsQueue, 1, &submitInfo, m_impl->inFlightFences[m_impl->currentFrame]));

    m_impl->swapchain->present(m_impl->presentQueue, m_impl->renderFinishedSemaphores[m_impl->currentFrame]);

    m_impl->currentFrame = (m_impl->currentFrame + 1) % Impl::MAX_FRAMES_IN_FLIGHT;
}

void          DeviceVK::waitIdle() {
    if (m_impl && m_impl->device) {
        vkDeviceWaitIdle(m_impl->device);
    }
}

void          DeviceVK::setVsync(bool vsync) {
    if (m_impl->vsync != vsync) {
        m_impl->vsync = vsync;
        if (m_impl->swapchain) {
            auto extent = m_impl->swapchain->getExtent();
            m_impl->swapchain->recreate(extent.width, extent.height, m_impl->vsync);
        }
    }
}

} // namespace mc::render
