#pragma once
#include "../IRenderDevice.h"
#include <windows.h>
#include <vector>
#include <optional>

namespace mc::render {

class DeviceVK : public IRenderDevice {
public:
    static std::unique_ptr<DeviceVK> create(HWND hwnd);

    ~DeviceVK() override;

    IBuffer*      createBuffer(const BufferDesc&) override;
    ITexture*     createTexture(const TextureDesc&) override;
    IPipeline*    createPipeline(const PipelineDesc&) override;

    void destroyBuffer(IBuffer*) override;
    void destroyTexture(ITexture*) override;
    void destroyPipeline(IPipeline*) override;

    ICommandList* beginFrame(int32_t viewW, int32_t viewH) override;
    void          endFrame() override;
    void          waitIdle() override;
    void          setVsync(bool vsync) override;
    std::string_view backendName() const override { return "Vulkan 1.3"; }

private:
    DeviceVK() = default;
    bool init(HWND hwnd);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace mc::render
