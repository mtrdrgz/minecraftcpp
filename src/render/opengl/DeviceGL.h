#pragma once
#include "../IRenderDevice.h"
#include <windows.h>
#include <memory>

namespace mc::render {

class CommandListGL;

class DeviceGL final : public IRenderDevice {
public:
    // Creates WGL context on the given HWND, loads GLAD, returns ready device
    static std::unique_ptr<DeviceGL> create(HWND hwnd, bool vsync = true);
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
    bool init(HWND hwnd, bool vsync);

    HWND    m_hwnd    = nullptr;
    HDC     m_hdc     = nullptr;
    HGLRC   m_hglrc   = nullptr;
    std::unique_ptr<CommandListGL> m_cmdList;
};

} // namespace mc::render
