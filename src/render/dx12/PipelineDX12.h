#pragma once
#include "../IRenderDevice.h"
#include "d3dx12_mini.h"
#include <wrl/client.h>
#include <string>
#include <map>

namespace mc::render {

using Microsoft::WRL::ComPtr;

class PipelineDX12 : public IPipeline {
public:
    PipelineDX12(ID3D12Device* device, const PipelineDesc& desc);
    ~PipelineDX12() override = default;

    ID3D12PipelineState* getPipelineState() const { return m_pipelineState.Get(); }
    ID3D12RootSignature* getRootSignature() const { return m_rootSignature.Get(); }
    
    VertexLayout getLayout() const { return m_layout; }

private:
    void createRootSignature();
    void createPipelineState(const PipelineDesc& desc);

    ComPtr<ID3D12Device>        m_device;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    
    BlendMode    m_blend;
    DepthTest    m_depth;
    CullMode     m_cull;
    VertexLayout m_layout;
};

} // namespace mc::render
