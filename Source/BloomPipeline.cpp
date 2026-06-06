#include "Globals.h"
#include "BloomPipeline.h"
#include "ModuleSamplerHeap.h"
#include "ReadData.h"
#include <d3dx12.h>

bool BloomPipeline::init(ID3D12Device* device, DXGI_FORMAT sceneRTFormat) {
    return createComputeRootSig(device)
        && createCompositeRootSig(device)
        && createComputePSOs(device)
        && createCompositePSO(device, sceneRTFormat);
}

bool BloomPipeline::createComputeRootSig(ID3D12Device* device) {
    CD3DX12_DESCRIPTOR_RANGE srvRange;     srvRange    .Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE uavRange;     uavRange    .Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE samplerRange; samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
                                                             ModuleSamplerHeap::COUNT, 0);

    CD3DX12_ROOT_PARAMETER params[4];
    params[CS_SLOT_CB     ].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);
    params[CS_SLOT_INPUT  ].InitAsDescriptorTable(1, &srvRange,     D3D12_SHADER_VISIBILITY_ALL);
    params[CS_SLOT_OUTPUT ].InitAsDescriptorTable(1, &uavRange,     D3D12_SHADER_VISIBILITY_ALL);
    params[CS_SLOT_SAMPLER].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(params), params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    if (FAILED(hr)) {
        if (error) OutputDebugStringA((char*)error->GetBufferPointer());
        LOG("BloomPipeline: compute root sig serialize failed 0x%08X", hr);
        return false;
    }
    hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                      IID_PPV_ARGS(&m_computeRootSig));
    if (FAILED(hr)) { LOG("BloomPipeline: CreateRootSignature failed 0x%08X", hr); return false; }
    return true;
}

bool BloomPipeline::createCompositeRootSig(ID3D12Device* device) {
    CD3DX12_DESCRIPTOR_RANGE srvRange;     srvRange    .Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE samplerRange; samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
                                                             ModuleSamplerHeap::COUNT, 0);
    CD3DX12_ROOT_PARAMETER params[2];
    params[GFX_SLOT_BLOOM_SRV].InitAsDescriptorTable(1, &srvRange,     D3D12_SHADER_VISIBILITY_PIXEL);
    params[GFX_SLOT_SAMPLER  ].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_PIXEL);

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
                                      IID_PPV_ARGS(&m_compositeRootSig));
    if (FAILED(hr)) { LOG("BloomPipeline: composite root sig failed 0x%08X", hr); return false; }
    return true;
}

bool BloomPipeline::createComputePSOs(ID3D12Device* device) {
    auto makeCSPSO = [&](const wchar_t* cso, ComPtr<ID3D12PipelineState>& out) -> bool {
        auto cs = DX::ReadData(cso);
        D3D12_COMPUTE_PIPELINE_STATE_DESC d = {};
        d.pRootSignature = m_computeRootSig.Get();
        d.CS = { cs.data(), cs.size() };
        HRESULT hr = device->CreateComputePipelineState(&d, IID_PPV_ARGS(&out));
        if (FAILED(hr)) { LOG("BloomPipeline: CS PSO failed %ls 0x%08X", cso, hr); return false; }
        return true;
    };
    return makeCSPSO(L"BloomThresholdCS.cso", m_thresholdPSO)
        && makeCSPSO(L"BloomBlurHCS.cso",     m_blurHPSO)
        && makeCSPSO(L"BloomBlurVCS.cso",     m_blurVPSO);
}

bool BloomPipeline::createCompositePSO(ID3D12Device* device, DXGI_FORMAT sceneRTFormat) {
    auto vs = DX::ReadData(L"FullScreenVS.cso");
    auto ps = DX::ReadData(L"BloomCompositePS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC d = {};
    d.pRootSignature       = m_compositeRootSig.Get();
    d.VS                   = { vs.data(), vs.size() };
    d.PS                   = { ps.data(), ps.size() };
    d.InputLayout          = { nullptr, 0 };
    d.PrimitiveTopologyType= D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    d.NumRenderTargets     = 1;
    d.RTVFormats[0]        = sceneRTFormat;
    d.DSVFormat            = DXGI_FORMAT_UNKNOWN;
    d.SampleDesc           = { 1, 0 };
    d.SampleMask           = UINT_MAX;

    d.RasterizerState      = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    // Additive blend: dst = src.rgb + dst.rgb
    d.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    d.BlendState.RenderTarget[0].BlendEnable    = TRUE;
    d.BlendState.RenderTarget[0].SrcBlend       = D3D12_BLEND_ONE;
    d.BlendState.RenderTarget[0].DestBlend      = D3D12_BLEND_ONE;
    d.BlendState.RenderTarget[0].BlendOp        = D3D12_BLEND_OP_ADD;
    d.BlendState.RenderTarget[0].SrcBlendAlpha  = D3D12_BLEND_ZERO;
    d.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
    d.BlendState.RenderTarget[0].BlendOpAlpha   = D3D12_BLEND_OP_ADD;
    d.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED |
                                                          D3D12_COLOR_WRITE_ENABLE_GREEN |
                                                          D3D12_COLOR_WRITE_ENABLE_BLUE;

    d.DepthStencilState            = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    d.DepthStencilState.DepthEnable= FALSE;
    d.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    HRESULT hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&m_compositePSO));
    if (FAILED(hr)) { LOG("BloomPipeline: composite PSO failed 0x%08X", hr); return false; }
    return true;
}
