#pragma once
#include "../IRenderDevice.h"
#include <memory>

#ifdef _WIN32
#include <windows.h>
#endif

namespace mc::render {

class CommandListGL;

class DeviceGL final : public IRenderDevice {
public:
#ifdef _WIN32
    // Windows: creates WGL context on the given HWND
    static std::unique_ptr<DeviceGL> create(HWND hwnd, bool vsync = true);
#else
    // Linux: context is already created by GLFW; just loads GL functions
    static std::unique_ptr<DeviceGL> create(void* glfwWindow, bool vsync = true);
#endif
    ~DeviceGL() override;

    IBuffer*      createBuffer(const BufferDesc&)    override;
    ITexture*     createTexture(const TextureDesc&)   override;
    IPipeline*    createPipeline(const PipelineDesc&) override;
    void          destroyBuffer(IBuffer*)   override;
    void          destroyTexture(ITexture*) override;
    void          destroyPipeline(IPipeline*) override;
    ICommandList* beginFrame(int32_t w, int32_t h) override;
    void          endFrame() override;
    void          waitIdle() override;
    void          setVsync(bool) override;
    std::string_view backendName() const override { return "OpenGL 4.6"; }

private:
    DeviceGL() = default;
#ifdef _WIN32
    bool init(HWND hwnd, bool vsync);
    HWND    m_hwnd    = nullptr;
    HDC     m_hdc     = nullptr;
    HGLRC   m_hglrc   = nullptr;
#else
    bool init(void* glfwWindow, bool vsync);
    void*   m_window  = nullptr;  // GLFWwindow*
#endif
    std::unique_ptr<CommandListGL> m_cmdList;
};

} // namespace mc::render
