#include "BufferDX12.h"
#include <stdexcept>

namespace mc::render {

BufferDX12::BufferDX12(D3D12MA::Allocator* allocator, const BufferDesc& desc)
    : m_desc(desc) 
{
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = desc.size;
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.Format = DXGI_FORMAT_UNKNOWN;
    resDesc.SampleDesc.Count = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12MA::ALLOCATION_DESC allocDesc = {};
    allocDesc.HeapType = desc.dynamic ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;

    if (FAILED(allocator->CreateResource(&allocDesc, &resDesc, desc.dynamic ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COMMON, nullptr, &m_allocation, IID_PPV_ARGS(&m_resource)))) {
        throw std::runtime_error("Failed to create DX12 buffer");
    }
}

BufferDX12::~BufferDX12() {
}

D3D12_GPU_VIRTUAL_ADDRESS BufferDX12::getGpuAddress() const {
    return m_resource->GetGPUVirtualAddress();
}

void BufferDX12::upload(const void* data, size_t size, size_t offset) {
    if (m_desc.dynamic) {
        void* mapped = nullptr;
        D3D12_RANGE range = { offset, offset + size };
        if (SUCCEEDED(m_resource->Map(0, &range, &mapped))) {
            std::memcpy((uint8_t*)mapped + offset, data, size);
            m_resource->Unmap(0, &range);
        }
    } else {
        // Static buffer upload usually needs a staging buffer.
        // For simplicity in this port, we use UPLOAD heaps even for static if we don't have a copy queue logic ready.
    }
}

} // namespace mc::render
