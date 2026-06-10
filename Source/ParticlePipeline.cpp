#include "Globals.h"
#include "ParticlePipeline.h"
#include "ModuleSamplerHeap.h"
#include "ReadData.h"
#include <d3dx12.h>

bool ParticlePipeline::init(ID3D12Device* device)
{
    return createGfxRootSignature(device)
        && createCsRootSignature(device)
        && createGraphicsPSOs(device)
        && createComputePSO(device);
}

bool ParticlePipeline::createGfxRootSignature(ID3D12Device* device)
{
    // t0 = GpuParticle StructuredBuffer (1 SRV),  t1 = texture (1 SRV)
    CD3DX12_DESCRIPTOR_RANGE particleRange;
    particleRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0

    CD3DX12_DESCRIPTOR_RANGE textureRange;
    textureRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);  // t1

    CD3DX12_DESCRIPTOR_RANGE samplerRange;
    samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
                      ModuleSamplerHeap::COUNT, 0);             // s0..sN

    CD3DX12_ROOT_PARAMETER params[4];
    params[GFX_SLOT_CB].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
    params[GFX_SLOT_PARTICLES].InitAsDescriptorTable(1, &particleRange, D3D12_SHADER_VISIBILITY_VERTEX);
    params[GFX_SLOT_TEXTURE  ].InitAsDescriptorTable(1, &textureRange,  D3D12_SHADER_VISIBILITY_PIXEL);
    params[GFX_SLOT_SAMPLER  ].InitAsDescriptorTable(1, &samplerRange,  D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(params), params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    if (FAILED(hr)) {
        if (error) OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
        LOG("ParticlePipeline: serialize gfx root sig failed 0x%08X", hr);
        return false;
    }
    hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                      IID_PPV_ARGS(&m_gfxRootSig));
    if (FAILED(hr)) { LOG("ParticlePipeline: CreateRootSignature (gfx) failed 0x%08X", hr); return false; }
    return true;
}

bool ParticlePipeline::createCsRootSignature(ID3D12Device* device)
{
    CD3DX12_DESCRIPTOR_RANGE inputRange;
    inputRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);  // t0 — input SRV

    CD3DX12_DESCRIPTOR_RANGE outputRange;
    outputRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0); // u0 — output UAV

    CD3DX12_ROOT_PARAMETER params[3];
    params[CS_SLOT_CB    ].InitAsConstantBufferView(0);
    params[CS_SLOT_INPUT ].InitAsDescriptorTable(1, &inputRange);
    params[CS_SLOT_OUTPUT].InitAsDescriptorTable(1, &outputRange);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(params), params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    if (FAILED(hr)) {
        if (error) OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
        LOG("ParticlePipeline: serialize cs root sig failed 0x%08X", hr);
        return false;
    }
    hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                      IID_PPV_ARGS(&m_csRootSig));
    if (FAILED(hr)) { LOG("ParticlePipeline: CreateRootSignature (cs) failed 0x%08X", hr); return false; }
    return true;
}

bool ParticlePipeline::createGraphicsPSOs(ID3D12Device* device)
{
    auto vs = DX::ReadData(L"ParticleRenderVS.cso");
    auto ps = DX::ReadData(L"ParticleRenderPS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = m_gfxRootSig.Get();
    desc.VS = { vs.data(), vs.size() };
    desc.PS = { ps.data(), ps.size() };
    desc.InputLayout = { nullptr, 0 };  // no vertex buffer; positions come from StructuredBuffer
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.DSVFormat     = DXGI_FORMAT_D32_FLOAT;
    desc.SampleDesc    = { 1, 0 };
    desc.SampleMask    = UINT_MAX;

    desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    // Alpha blend: src*srcAlpha + dst*(1-srcAlpha)
    desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    auto& rt = desc.BlendState.RenderTarget[0];
    rt.BlendEnable    = TRUE;
    rt.SrcBlend       = D3D12_BLEND_SRC_ALPHA;
    rt.DestBlend      = D3D12_BLEND_INV_SRC_ALPHA;
    rt.BlendOp        = D3D12_BLEND_OP_ADD;
    rt.SrcBlendAlpha  = D3D12_BLEND_ONE;
    rt.DestBlendAlpha = D3D12_BLEND_ZERO;
    rt.BlendOpAlpha   = D3D12_BLEND_OP_ADD;
    rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // Depth test ON, write OFF — share GBuffer read-only depth.
    desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    desc.DepthStencilState.DepthEnable    = TRUE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState.DepthFunc      = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    HRESULT hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_pso));
    if (FAILED(hr)) { LOG("ParticlePipeline: alpha PSO failed 0x%08X", hr); return false; }

    // Additive variant: src*srcAlpha + dst
    auto& art = desc.BlendState.RenderTarget[0];
    art.DestBlend = D3D12_BLEND_ONE;
    hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_additivePso));
    if (FAILED(hr)) { LOG("ParticlePipeline: additive PSO failed 0x%08X", hr); return false; }

    return true;
}

bool ParticlePipeline::createComputePSO(ID3D12Device* device)
{
    auto cs = DX::ReadData(L"ParticleUpdateCS.cso");

    D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = m_csRootSig.Get();
    desc.CS = { cs.data(), cs.size() };

    HRESULT hr = device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&m_computePso));
    if (FAILED(hr)) { LOG("ParticlePipeline: compute PSO failed 0x%08X", hr); return false; }
    return true;
}
