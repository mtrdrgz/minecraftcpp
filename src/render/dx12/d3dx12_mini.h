#pragma once
#include <d3d12.h>

struct D3D12_DEFAULT_TAG {};
#define D3D12_DEFAULT D3D12_DEFAULT_TAG()

struct CD3DX12_RASTERIZER_DESC : public D3D12_RASTERIZER_DESC {
    CD3DX12_RASTERIZER_DESC() = default;
    explicit CD3DX12_RASTERIZER_DESC(const D3D12_RASTERIZER_DESC& o) : D3D12_RASTERIZER_DESC(o) {}
    explicit CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT_TAG) {
        FillMode = D3D12_FILL_MODE_SOLID;
        CullMode = D3D12_CULL_MODE_BACK;
        FrontCounterClockwise = FALSE;
        DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        DepthClipEnable = TRUE;
        MultisampleEnable = FALSE;
        AntialiasedLineEnable = FALSE;
        ForcedSampleCount = 0;
        ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    }
};

struct CD3DX12_BLEND_DESC : public D3D12_BLEND_DESC {
    CD3DX12_BLEND_DESC() = default;
    explicit CD3DX12_BLEND_DESC(const D3D12_BLEND_DESC& o) : D3D12_BLEND_DESC(o) {}
    explicit CD3DX12_BLEND_DESC(D3D12_DEFAULT_TAG) {
        AlphaToCoverageEnable = FALSE;
        IndependentBlendEnable = FALSE;
        const D3D12_RENDER_TARGET_BLEND_DESC defaultRTV = {
            FALSE,FALSE,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL,
        };
        for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
            RenderTarget[i] = defaultRTV;
    }
};

struct CD3DX12_DEPTH_STENCIL_DESC : public D3D12_DEPTH_STENCIL_DESC {
    CD3DX12_DEPTH_STENCIL_DESC() = default;
    explicit CD3DX12_DEPTH_STENCIL_DESC(const D3D12_DEPTH_STENCIL_DESC& o) : D3D12_DEPTH_STENCIL_DESC(o) {}
    explicit CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT_TAG) {
        DepthEnable = TRUE;
        DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        StencilEnable = FALSE;
        StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
        const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
        FrontFace = defaultStencilOp;
        BackFace = defaultStencilOp;
    }
};

struct CD3DX12_RESOURCE_BARRIER : public D3D12_RESOURCE_BARRIER {
    CD3DX12_RESOURCE_BARRIER() = default;
    explicit CD3DX12_RESOURCE_BARRIER(const D3D12_RESOURCE_BARRIER& o) : D3D12_RESOURCE_BARRIER(o) {}
    static inline CD3DX12_RESOURCE_BARRIER TransitionBarrier(
        ID3D12Resource* pRes,
        D3D12_RESOURCE_STATES before,
        D3D12_RESOURCE_STATES after,
        UINT sub = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        D3D12_RESOURCE_BARRIER_FLAGS f = D3D12_RESOURCE_BARRIER_FLAG_NONE)
    {
        CD3DX12_RESOURCE_BARRIER r = {};
        r.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        r.Flags = f;
        r.Transition.pResource = pRes;
        r.Transition.StateBefore = before;
        r.Transition.StateAfter = after;
        r.Transition.Subresource = sub;
        return r;
    }
};

struct CD3DX12_SHADER_BYTECODE : public D3D12_SHADER_BYTECODE {
    CD3DX12_SHADER_BYTECODE() = default;
    CD3DX12_SHADER_BYTECODE(const D3D12_SHADER_BYTECODE& o) : D3D12_SHADER_BYTECODE(o) {}
    CD3DX12_SHADER_BYTECODE(ID3DBlob* pBlob) {
        pShaderBytecode = pBlob->GetBufferPointer();
        BytecodeLength = pBlob->GetBufferSize();
    }
};
