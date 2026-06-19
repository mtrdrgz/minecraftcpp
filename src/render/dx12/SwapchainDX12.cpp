#include "SwapchainDX12.h"
#include <stdexcept>

namespace mc::render {

SwapchainDX12::SwapchainDX12(ID3D12Device* device, IDXGIFactory4* factory, ID3D12CommandQueue* queue, HWND hwnd, uint32_t width, uint32_t height, uint32_t bufferCount)
    : m_bufferCount(bufferCount)
{
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = m_bufferCount;
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    if (FAILED(factory->CreateSwapChainForHwnd(queue, hwnd, &swapChainDesc, nullptr, nullptr, &swapChain))) {
        throw std::runtime_error("Failed to create DXGI swapchain");
    }

    if (FAILED(swapChain.As(&m_swapChain))) {
        throw std::runtime_error("Failed to cast swapchain to IDXGISwapChain3");
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    createRtv(device);
}

SwapchainDX12::~SwapchainDX12() {
    releaseRtv();
}

void SwapchainDX12::createRtv(ID3D12Device* device) {
    // Describe and create a render target view (RTV) descriptor heap.
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = m_bufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)))) {
        throw std::runtime_error("Failed to create RTV descriptor heap");
    }

    m_rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Create frame resources.
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

    m_backBuffers.resize(m_bufferCount);
    for (uint32_t n = 0; n < m_bufferCount; n++) {
        if (FAILED(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_backBuffers[n])))) {
            throw std::runtime_error("Failed to get swapchain buffer");
        }
        device->CreateRenderTargetView(m_backBuffers[n].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += m_rtvDescriptorSize;
    }
}

void SwapchainDX12::releaseRtv() {
    m_backBuffers.clear();
    m_rtvHeap.Reset();
}

void SwapchainDX12::present(bool vsync) {
    UINT syncInterval = vsync ? 1 : 0;
    UINT flags = 0;

    if (FAILED(m_swapChain->Present(syncInterval, flags))) {
        throw std::runtime_error("Failed to present swapchain");
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void SwapchainDX12::resize(ID3D12Device* device, uint32_t width, uint32_t height) {
    // 1. Release all references to back buffers
    releaseRtv();

    // 2. Resize swapchain
    if (FAILED(m_swapChain->ResizeBuffers(m_bufferCount, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0))) {
        throw std::runtime_error("Failed to resize swapchain buffers");
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // 3. Recreate RTVs
    createRtv(device);
}

ID3D12Resource* SwapchainDX12::currentBackBuffer() const {
    return m_backBuffers[m_frameIndex].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE SwapchainDX12::currentRtvDescriptor() const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += (m_frameIndex * m_rtvDescriptorSize);
    return handle;
}

} // namespace mc::render
