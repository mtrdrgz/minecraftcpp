#pragma once

#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <vector>
#include <cstdint>

namespace mc::render {

using Microsoft::WRL::ComPtr;

class SwapchainDX12 {
public:
    SwapchainDX12(ID3D12Device* device, IDXGIFactory4* factory, ID3D12CommandQueue* queue, HWND hwnd, uint32_t width, uint32_t height, uint32_t bufferCount = 2);
    ~SwapchainDX12();

    void present(bool vsync);
    void resize(ID3D12Device* device, uint32_t width, uint32_t height);

    ID3D12Resource* currentBackBuffer() const;
    D3D12_CPU_DESCRIPTOR_HANDLE currentRtvDescriptor() const;
    uint32_t currentBackBufferIndex() const { return m_frameIndex; }
    uint32_t bufferCount() const { return m_bufferCount; }

private:
    void createRtv(ID3D12Device* device);
    void releaseRtv();

    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    std::vector<ComPtr<ID3D12Resource>> m_backBuffers;
    uint32_t m_rtvDescriptorSize = 0;
    uint32_t m_frameIndex = 0;
    uint32_t m_bufferCount = 0;
};

} // namespace mc::render
