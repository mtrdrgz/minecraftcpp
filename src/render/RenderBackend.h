#pragma once
#include "IRenderDevice.h"
#include <string>

#ifdef _WIN32
#include <windows.h>
typedef HWND NativeWindowHandle;
#else
typedef void* NativeWindowHandle;
#endif

namespace mc::render {

enum class BackendType {
    OpenGL,
    Vulkan,
    DirectX12
};

struct RenderBackend {
    static std::unique_ptr<IRenderDevice> createDevice(BackendType type, NativeWindowHandle hwnd);
    static BackendType typeFromString(std::string_view str);
};

} // namespace mc::render
