#include "PipelineDX12.h"
#include "../../core/Log.h"
#include <d3dcompiler.h>
#include <stdexcept>
#include <vector>

namespace mc::render {

PipelineDX12::PipelineDX12(ID3D12Device* device, const PipelineDesc& desc)
    : m_device(device), m_blend(desc.blend), m_depth(desc.depth), m_cull(desc.cull), m_layout(desc.layout)
{
    createRootSignature();
    createPipelineState(desc);
}

void PipelineDX12::createRootSignature() {
    D3D12_ROOT_PARAMETER rootParameters[1] = {};
    
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParameters[0].Constants.ShaderRegister = 0;
    rootParameters[0].Constants.RegisterSpace  = 0;
    rootParameters[0].Constants.Num32BitValues = 32;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = 1;
    rootSigDesc.pParameters = rootParameters;
    rootSigDesc.NumStaticSamplers = 1;
    rootSigDesc.pStaticSamplers = &sampler;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    if (FAILED(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error))) {
        if (error) MC_LOG_ERROR("DX12 Root Signature error: {}", (const char*)error->GetBufferPointer());
        throw std::runtime_error("Failed to serialize DX12 root signature");
    }
    m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
}

void PipelineDX12::createPipelineState(const PipelineDesc& desc) {
    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;
    ComPtr<ID3DBlob> error;

    UINT compileFlags = 0;
#if defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    if (FAILED(D3DCompile(desc.vsSource.data(), desc.vsSource.size(), nullptr, nullptr, nullptr, "main", "vs_5_0", compileFlags, 0, &vertexShader, &error))) {
        if (error) MC_LOG_ERROR("VS Compile Error: {}", (const char*)error->GetBufferPointer());
        throw std::runtime_error("Failed to compile vertex shader");
    }

    if (FAILED(D3DCompile(desc.fsSource.data(), desc.fsSource.size(), nullptr, nullptr, nullptr, "main", "ps_5_0", compileFlags, 0, &pixelShader, &error))) {
        if (error) MC_LOG_ERROR("PS Compile Error: {}", (const char*)error->GetBufferPointer());
        throw std::runtime_error("Failed to compile pixel shader");
    }

    std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescs;
    if (desc.layout == VertexLayout::PositionTexColor) {
        inputElementDescs = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };
    } else if (desc.layout == VertexLayout::Gui) {
        inputElementDescs = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };
    } else {
        inputElementDescs = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs.data(), (UINT)inputElementDescs.size() };
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
    psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
    
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    if (desc.cull == CullMode::None) psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    else if (desc.cull == CullMode::Front) psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
    
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    if (desc.blend != BlendMode::None) {
        psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
        psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    }

    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    if (desc.depth == DepthTest::Disabled) psoDesc.DepthStencilState.DepthEnable = FALSE;
    else if (desc.depth == DepthTest::ReadOnly) psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = (desc.primitive == PrimitiveType::Lines) ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE : D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleDesc.Count = 1;

    if (FAILED(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)))) {
        throw std::runtime_error("Failed to create DX12 pipeline state");
    }
}

} // namespace mc::render
