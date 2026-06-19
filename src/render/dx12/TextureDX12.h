#pragma once
#include "../IRenderDevice.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <D3D12MemAlloc.h>

namespace mc::render {

using Microsoft::WRL::ComPtr;

class TextureDX12 : public ITexture {
public:
    TextureDX12(D3D12MA::Allocator* allocator, const TextureDesc& desc);
    ~TextureDX12() override = default;

    ID3D12Resource* getResource() const { return m_allocation->GetResource(); }
    D3D12MA::Allocation* getAllocation() const { return m_allocation.Get(); }
    const TextureDesc& getDesc() const { return m_desc; }

    DXGI_FORMAT getDXGIFormat() const;

private:
    TextureDesc m_desc;
    ComPtr<D3D12MA::Allocation> m_allocation;
};

} // namespace mc::render
