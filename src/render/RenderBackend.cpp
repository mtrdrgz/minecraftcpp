#include "RenderBackend.h"

#include "opengl/DeviceGL.h"
#ifdef _WIN32
#include "vulkan/DeviceVK.h"
#include "dx12/DeviceDX12.h"
#endif

namespace mc::render {

std::unique_ptr<IRenderDevice> RenderBackend::createDevice(BackendType type, NativeWindowHandle hwnd) {
    switch (type) {
        case BackendType::OpenGL:
            return DeviceGL::create(hwnd, false);
        case BackendType::Vulkan:
#ifdef _WIN32
            return DeviceVK::create(hwnd);
#else
            return nullptr;  // Vulkan on Linux not yet implemented
#endif
        case BackendType::DirectX12:
#ifdef _WIN32
            return DeviceDX12::create(hwnd);
#else
            return nullptr;  // DX12 is Windows-only
#endif
        default: return nullptr;
    }
}

BackendType RenderBackend::typeFromString(std::string_view str) {
    if (str == "vulkan") return BackendType::Vulkan;
    if (str == "dx12")   return BackendType::DirectX12;
    return BackendType::OpenGL;
}

} // namespace mc::render
