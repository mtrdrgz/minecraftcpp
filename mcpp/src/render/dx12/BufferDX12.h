#pragma once
#include "../IRenderDevice.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <D3D12MemAlloc.h>

namespace mc::render {

using Microsoft::WRL::ComPtr;

class BufferDX12 : public IBuffer {
public:
    BufferDX12(D3D12MA::Allocator* allocator, const BufferDesc& desc);
    ~BufferDX12() override;

    D3D12_GPU_VIRTUAL_ADDRESS getGpuAddress() const;
    const BufferDesc& getDesc() const { return m_desc; }
    ID3D12Resource* getResource() const { return m_resource.Get(); }
    
    void upload(const void* data, size_t size, size_t offset);

private:
    BufferDesc m_desc;
    ComPtr<D3D12MA::Allocation> m_allocation;
    ComPtr<ID3D12Resource>      m_resource;
};

} // namespace mc::render
