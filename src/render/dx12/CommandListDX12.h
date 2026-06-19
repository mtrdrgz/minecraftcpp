#pragma once
#include "../IRenderDevice.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <string>

namespace mc::render {

using Microsoft::WRL::ComPtr;

class CommandListDX12 : public ICommandList {
public:
    explicit CommandListDX12(ID3D12Device* device);
    ~CommandListDX12() override = default;

    void reset();
    ID3D12GraphicsCommandList* getHandle() const { return m_commandList.Get(); }

    void clear(float r, float g, float b, float a, bool depth) override;
    void setViewport(int32_t x, int32_t y, int32_t w, int32_t h) override;
    void bindPipeline(IPipeline*) override;
    void bindVertexBuffer(IBuffer*, uint32_t stride) override;
    void bindIndexBuffer(IBuffer*, bool use32bit) override;
    void bindTexture(ITexture*, uint32_t slot) override;
    void setUniform4f(std::string_view name, float x, float y, float z, float w) override;
    void setUniformMat4(std::string_view name, const float* m) override;
    void draw(uint32_t vertexCount, uint32_t firstVertex) override;
    void drawIndexed(uint32_t indexCount, uint32_t firstIndex) override;
    void uploadBuffer(IBuffer*, const void* data, size_t size, size_t offset) override;
    void uploadTexture(ITexture*, const void* pixels) override;

private:
    void flushRootConstants();

    ComPtr<ID3D12CommandAllocator>    m_allocator;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    
    IPipeline* m_currentPipeline = nullptr;
    float      m_rootConstants[32]; // Enough for Mat4 + some vecs
    bool       m_rootConstantsDirty = false;
};

} // namespace mc::render
