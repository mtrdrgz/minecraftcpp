#include "DeviceDX12.h"
#include "BufferDX12.h"
#include "TextureDX12.h"
#include "PipelineDX12.h"
#include "CommandListDX12.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include "d3dx12_mini.h"
#include <iostream>
#include <vector>
#include <D3D12MemAlloc.h>

namespace mc::render {

struct DeviceDX12::Impl {
    ComPtr<ID3D12Device> device;
    ComPtr<IDXGIFactory4> factory;
    ComPtr<ID3D12CommandQueue> commandQueue;
    ComPtr<ID3D12Fence> fence;
    UINT64 fenceValue = 0;
    HANDLE fenceEvent = nullptr;
    ComPtr<D3D12MA::Allocator> allocator;

    ~Impl() {
        if (fenceEvent) CloseHandle(fenceEvent);
    }
};

std::unique_ptr<DeviceDX12> DeviceDX12::create(HWND hwnd) {
    auto device = std::unique_ptr<DeviceDX12>(new DeviceDX12());
    if (device->init(hwnd)) {
        return device;
    }
    return nullptr;
}

DeviceDX12::~DeviceDX12() {
    waitIdle();
}

bool DeviceDX12::init(HWND hwnd) {
    m_hwnd = hwnd;
    m_impl = std::make_unique<Impl>();

#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
    }
#endif

    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&m_impl->factory)))) return false;

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; m_impl->factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
        ComPtr<ID3D12Device> testDevice;
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&testDevice)))) break;
    }

    if (FAILED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_impl->device)))) return false;

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    if (FAILED(m_impl->device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_impl->commandQueue)))) return false;

    if (FAILED(m_impl->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_impl->fence)))) return false;
    m_impl->fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_impl->fenceEvent == nullptr) return false;

    D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
    allocatorDesc.pDevice = m_impl->device.Get();
    allocatorDesc.pAdapter = adapter.Get();
    if (FAILED(D3D12MA::CreateAllocator(&allocatorDesc, &m_impl->allocator))) return false;

    RECT rect; GetClientRect(hwnd, &rect);
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;

    m_swapchain = std::make_unique<SwapchainDX12>(m_impl->device.Get(), m_impl->factory.Get(), m_impl->commandQueue.Get(), hwnd, w, h);
    m_commandList = std::make_unique<CommandListDX12>(m_impl->device.Get());

    return true;
}

IBuffer* DeviceDX12::createBuffer(const BufferDesc& desc) {
    return new BufferDX12(m_impl->allocator.Get(), desc);
}

ITexture* DeviceDX12::createTexture(const TextureDesc& desc) {
    return new TextureDX12(m_impl->allocator.Get(), desc);
}

IPipeline* DeviceDX12::createPipeline(const PipelineDesc& desc) {
    return new PipelineDX12(m_impl->device.Get(), desc);
}

void DeviceDX12::destroyBuffer(IBuffer* b) { delete static_cast<BufferDX12*>(b); }
void DeviceDX12::destroyTexture(ITexture* t) { delete static_cast<TextureDX12*>(t); }
void DeviceDX12::destroyPipeline(IPipeline* p) { delete static_cast<PipelineDX12*>(p); }

ICommandList* DeviceDX12::beginFrame(int32_t viewW, int32_t viewH) {
    m_commandList->reset();
    
    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::TransitionBarrier(
        m_swapchain->currentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commandList->getHandle()->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_swapchain->currentRtvDescriptor();
    m_commandList->getHandle()->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    m_commandList->setViewport(0, 0, viewW, viewH);

    return m_commandList.get();
}

void DeviceDX12::endFrame() {
    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::TransitionBarrier(
        m_swapchain->currentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    m_commandList->getHandle()->ResourceBarrier(1, &barrier);

    m_commandList->getHandle()->Close();
    ID3D12CommandList* ppCommandLists[] = { m_commandList->getHandle() };
    m_impl->commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    m_swapchain->present(m_vsync);
    waitIdle();
}

void DeviceDX12::waitIdle() {
    if (!m_impl || !m_impl->device) return;
    const UINT64 fence = ++m_impl->fenceValue;
    m_impl->commandQueue->Signal(m_impl->fence.Get(), fence);
    if (m_impl->fence->GetCompletedValue() < fence) {
        m_impl->fence->SetEventOnCompletion(fence, m_impl->fenceEvent);
        WaitForSingleObject(m_impl->fenceEvent, INFINITE);
    }
}

void DeviceDX12::setVsync(bool vsync) {
    m_vsync = vsync;
}

} // namespace mc::render
