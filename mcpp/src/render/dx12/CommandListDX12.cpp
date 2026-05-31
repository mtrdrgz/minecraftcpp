#include "CommandListDX12.h"
#include "BufferDX12.h"
#include "TextureDX12.h"
#include "PipelineDX12.h"
#include "d3dx12_mini.h"
#include <algorithm>
#include <cstring>

namespace mc::render {

CommandListDX12::CommandListDX12(ID3D12Device* device) {
    // Create a dedicated command allocator for each frame would be better,
    // but for the prototype we'll use a single one and wait for idle.
    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_allocator));
    device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_allocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList));
    m_commandList->Close();

    std::memset(m_rootConstants, 0, sizeof(m_rootConstants));
}

void CommandListDX12::reset() {
    m_allocator->Reset();
    m_commandList->Reset(m_allocator.Get(), nullptr);
    m_rootConstantsDirty = true;
}

void CommandListDX12::clear(float r, float g, float b, float a, bool depth) {
    // In DX12 we usually clear the RTV/DSV handles.
    // For this prototype, we'll implement this if we track the handles.
}

void CommandListDX12::setViewport(int32_t x, int32_t y, int32_t w, int32_t h) {
    D3D12_VIEWPORT viewport = { (float)x, (float)y, (float)w, (float)h, 0.0f, 1.0f };
    D3D12_RECT scissor = { x, y, x + w, y + h };
    m_commandList->RSSetViewports(1, &viewport);
    m_commandList->RSSetScissorRects(1, &scissor);
}

void CommandListDX12::bindPipeline(IPipeline* pipeline) {
    m_currentPipeline = pipeline;
    auto* pDX12 = static_cast<PipelineDX12*>(pipeline);
    m_commandList->SetPipelineState(pDX12->getPipelineState());
    m_commandList->SetGraphicsRootSignature(pDX12->getRootSignature());
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_rootConstantsDirty = true;
}

void CommandListDX12::bindVertexBuffer(IBuffer* buffer, uint32_t stride) {
    auto* bDX12 = static_cast<BufferDX12*>(buffer);
    D3D12_VERTEX_BUFFER_VIEW vbView = {};
    vbView.BufferLocation = bDX12->getGpuAddress();
    vbView.SizeInBytes = (UINT)bDX12->getDesc().size;
    vbView.StrideInBytes = stride;
    m_commandList->IASetVertexBuffers(0, 1, &vbView);
}

void CommandListDX12::bindIndexBuffer(IBuffer* buffer, bool use32bit) {
    auto* bDX12 = static_cast<BufferDX12*>(buffer);
    D3D12_INDEX_BUFFER_VIEW ibView = {};
    ibView.BufferLocation = bDX12->getGpuAddress();
    ibView.SizeInBytes = (UINT)bDX12->getDesc().size;
    ibView.Format = use32bit ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
    m_commandList->IASetIndexBuffer(&ibView);
}

void CommandListDX12::bindTexture(ITexture*, uint32_t) {
    // Stub
}

void CommandListDX12::setUniform4f(std::string_view name, float x, float y, float z, float w) {
    // Simple fixed layout for prototype
    if (name.find("Color") != std::string_view::npos) {
        m_rootConstants[16] = x; m_rootConstants[17] = y; m_rootConstants[18] = z; m_rootConstants[19] = w;
        m_rootConstantsDirty = true;
    }
}

void CommandListDX12::setUniformMat4(std::string_view name, const float* m) {
    if (name.find("MVP") != std::string_view::npos || name.find("Model") != std::string_view::npos) {
        std::memcpy(&m_rootConstants[0], m, 16 * sizeof(float));
        m_rootConstantsDirty = true;
    }
}

void CommandListDX12::flushRootConstants() {
    if (m_rootConstantsDirty) {
        m_commandList->SetGraphicsRoot32BitConstants(0, 32, m_rootConstants, 0);
        m_rootConstantsDirty = false;
    }
}

void CommandListDX12::draw(uint32_t vertexCount, uint32_t firstVertex) {
    flushRootConstants();
    m_commandList->DrawInstanced(vertexCount, 1, firstVertex, 0);
}

void CommandListDX12::drawIndexed(uint32_t indexCount, uint32_t firstIndex) {
    flushRootConstants();
    m_commandList->DrawIndexedInstanced(indexCount, 1, firstIndex, 0, 0);
}

void CommandListDX12::uploadBuffer(IBuffer* buffer, const void* data, size_t size, size_t offset) {
    static_cast<BufferDX12*>(buffer)->upload(data, size, offset);
}

void CommandListDX12::uploadTexture(ITexture*, const void*) {
    // Stub
}

} // namespace mc::render
