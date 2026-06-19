#include "TextureDX12.h"
#include <stdexcept>

namespace mc::render {

DXGI_FORMAT TextureDX12::getDXGIFormat() const {
    switch (m_desc.format) {
        case TextureFormat::RGBA8:           return DXGI_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::RGB8:            return DXGI_FORMAT_R8G8B8A8_UNORM; // Map to RGBA8 for compatibility
        case TextureFormat::R8:              return DXGI_FORMAT_R8_UNORM;
        case TextureFormat::Depth24Stencil8: return DXGI_FORMAT_D24_UNORM_S8_UINT;
        default:                             return DXGI_FORMAT_UNKNOWN;
    }
}

TextureDX12::TextureDX12(D3D12MA::Allocator* allocator, const TextureDesc& desc)
    : m_desc(desc)
{
    DXGI_FORMAT format = getDXGIFormat();
    
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Alignment          = 0;
    resourceDesc.Width              = desc.width;
    resourceDesc.Height             = desc.height;
    resourceDesc.DepthOrArraySize   = 1;
    resourceDesc.MipLevels          = static_cast<UINT16>(desc.mipLevels);
    resourceDesc.Format             = format;
    resourceDesc.SampleDesc.Count   = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;

    if (desc.format == TextureFormat::Depth24Stencil8) {
        resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    }

    D3D12MA::ALLOCATION_DESC allocDesc = {};
    allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
    if (desc.format == TextureFormat::Depth24Stencil8) {
        initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    }

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = format;
    if (desc.format == TextureFormat::Depth24Stencil8) {
        clearValue.DepthStencil.Depth   = 1.0f;
        clearValue.DepthStencil.Stencil = 0;
    } else {
        clearValue.Color[0] = 0.0f;
        clearValue.Color[1] = 0.0f;
        clearValue.Color[2] = 0.0f;
        clearValue.Color[3] = 0.0f;
    }

    HRESULT hr = allocator->CreateResource(
        &allocDesc,
        &resourceDesc,
        initialState,
        (desc.format == TextureFormat::Depth24Stencil8) ? &clearValue : nullptr,
        &m_allocation,
        IID_NULL, nullptr
    );

    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create DX12 texture");
    }
}

} // namespace mc::render
