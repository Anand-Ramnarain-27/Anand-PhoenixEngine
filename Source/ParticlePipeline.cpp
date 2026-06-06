#include "Globals.h"
#include "ParticlePipeline.h"
#include "ModuleSamplerHeap.h"
#include "ReadData.h"
#include <d3dx12.h>

bool ParticlePipeline::init(ID3D12Device* device, DXGI_FORMAT rtFormat, DXGI_FORMAT dsFormat) {
    return createComputeRootSig(device)
        && createGraphicsRootSig(device)
        && createUpdatePSO(device)
        && createRenderPSO(device, rtFormat, dsFormat);
}

bool ParticlePipeline::createComputeRootSig(ID3D12Device* device) {
    CD3DX12_DESCRIPTOR_RANGE particleUAV;  particleUAV .Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE deadUAV;      deadUAV     .Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);

    CD3DX12_ROOT_PARAMETER params[3];
    params[CS_SLOT_CB        ].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);
    params[CS_SLOT_PARTICLES ].InitAsDescriptorTable(1, &particleUAV, D3D12_SHADER_VISIBILITY_ALL);
    params[CS_SLOT_DEAD_COUNT].InitAsDescriptorTable(1, &deadUAV,     D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(params), params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    if (FAILED(hr)) {
        if (error) OutputDebugStringA((char*)error->GetBufferPointer());
        LOG("ParticlePipeline: CS root sig failed 0x%08X", hr);
        return false;
    }
    hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                      IID_PPV_ARGS(&m_csRootSig));
    if (FAILED(hr)) { LOG("ParticlePipeline: CreateRootSignature CS failed 0x%08X", hr); return false; }
    return true;
}

bool ParticlePipeline::createGraphicsRootSig(ID3D12Device* device) {
    CD3DX12_DESCRIPTOR_RANGE particleSRV;  particleSRV .Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE spriteSRV;    spriteSRV   .Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    CD3DX12_DESCRIPTOR_RANGE samplerRange; samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
                                                             ModuleSamplerHeap::COUNT, 0);

    CD3DX12_ROOT_PARAMETER params[4];
    params[GFX_SLOT_CB       ].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
    params[GFX_SLOT_PARTICLES].InitAsDescriptorTable(1, &particleSRV, D3D12_SHADER_VISIBILITY_VERTEX);
    params[GFX_SLOT_SPRITE   ].InitAsDescriptorTable(1, &spriteSRV,   D3D12_SHADER_VISIBILITY_PIXEL);
    params[GFX_SLOT_SAMPLER  ].InitAsDescriptorTable(1, &samplerRange,D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(params), params, 0, nullptr,
              D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    if (FAILED(hr)) {
        if (error) OutputDebugStringA((char*)error->GetBufferPointer());
        return false;
    }
    hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                      IID_PPV_ARGS(&m_gfxRootSig));
    if (FAILED(hr)) { LOG("ParticlePipeline: CreateRootSignature GFX failed 0x%08X", hr); return false; }
    return true;
}

bool ParticlePipeline::createUpdatePSO(ID3D12Device* device) {
    auto cs = DX::ReadData(L"ParticleUpdateCS.cso");
    D3D12_COMPUTE_PIPELINE_STATE_DESC d = {};
    d.pRootSignature = m_csRootSig.Get();
    d.CS = { cs.data(), cs.size() };
    HRESULT hr = device->CreateComputePipelineState(&d, IID_PPV_ARGS(&m_updatePSO));
    if (FAILED(hr)) { LOG("ParticlePipeline: update PSO failed 0x%08X", hr); return false; }
    return true;
}

bool ParticlePipeline::createRenderPSO(ID3D12Device* device, DXGI_FORMAT rtFormat, DXGI_FORMAT dsFormat) {
    auto vs = DX::ReadData(L"ParticleRenderVS.cso");
    auto ps = DX::ReadData(L"ParticleRenderPS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC d = {};
    d.pRootSignature        = m_gfxRootSig.Get();
    d.VS                    = { vs.data(), vs.size() };
    d.PS                    = { ps.data(), ps.size() };
    d.InputLayout           = { nullptr, 0 };  // no vertex buffer — VS uses SV_VertexID/InstanceID
    d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    d.NumRenderTargets      = 1;
    d.RTVFormats[0]         = rtFormat;
    d.DSVFormat             = dsFormat;
    d.SampleDesc            = { 1, 0 };
    d.SampleMask            = UINT_MAX;

    d.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    // Alpha blend: premultiplied alpha for particles
    d.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    d.BlendState.RenderTarget[0].BlendEnable    = TRUE;
    d.BlendState.RenderTarget[0].SrcBlend       = D3D12_BLEND_SRC_ALPHA;
    d.BlendState.RenderTarget[0].DestBlend      = D3D12_BLEND_INV_SRC_ALPHA;
    d.BlendState.RenderTarget[0].BlendOp        = D3D12_BLEND_OP_ADD;
    d.BlendState.RenderTarget[0].SrcBlendAlpha  = D3D12_BLEND_ONE;
    d.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    d.BlendState.RenderTarget[0].BlendOpAlpha   = D3D12_BLEND_OP_ADD;
    d.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // Depth test (read-only — particles sort via alpha, not depth)
    d.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    d.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    d.DepthStencilState.DepthFunc      = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    HRESULT hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&m_renderPSO));
    if (FAILED(hr)) { LOG("ParticlePipeline: render PSO failed 0x%08X", hr); return false; }
    return true;
}
