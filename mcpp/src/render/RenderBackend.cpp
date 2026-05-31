#include "RenderBackend.h"
#include "opengl/DeviceGL.h"
#include "vulkan/DeviceVK.h"
#include "dx12/DeviceDX12.h"

namespace mc::render {

std::unique_ptr<IRenderDevice> RenderBackend::createDevice(BackendType type, HWND hwnd) {
    switch (type) {
        case BackendType::OpenGL: return DeviceGL::create(hwnd, false);
        case BackendType::Vulkan: return DeviceVK::create(hwnd);
        case BackendType::DirectX12: return DeviceDX12::create(hwnd);
        default: return nullptr;
    }
}

BackendType RenderBackend::typeFromString(std::string_view str) {
    if (str == "vulkan") return BackendType::Vulkan;
    if (str == "dx12")   return BackendType::DirectX12;
    return BackendType::OpenGL;
}

} // namespace mc::render
