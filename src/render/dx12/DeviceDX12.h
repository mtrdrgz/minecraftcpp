#pragma once
#include "../IRenderDevice.h"
#include "SwapchainDX12.h"
#include "CommandListDX12.h"
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <memory>
#include <vector>

namespace mc::render {

using Microsoft::WRL::ComPtr;

class DeviceDX12 : public IRenderDevice {
public:
    static std::unique_ptr<DeviceDX12> create(HWND hwnd);

    ~DeviceDX12() override;

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
    std::string_view backendName() const override { return "DirectX 12"; }

private:
    DeviceDX12() = default;
    bool init(HWND hwnd);

    struct Impl;
    std::unique_ptr<Impl> m_impl;

    HWND m_hwnd = nullptr;
    bool m_vsync = true;
    std::unique_ptr<SwapchainDX12> m_swapchain;
    std::unique_ptr<CommandListDX12> m_commandList;
};

} // namespace mc::render
