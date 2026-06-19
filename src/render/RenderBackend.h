#pragma once
#include "IRenderDevice.h"
#include <windows.h>
#include <string>

namespace mc::render {

enum class BackendType {
    OpenGL,
    Vulkan,
    DirectX12
};

struct RenderBackend {
    static std::unique_ptr<IRenderDevice> createDevice(BackendType type, HWND hwnd);
    static BackendType typeFromString(std::string_view str);
};

} // namespace mc::render
