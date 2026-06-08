#include "Globals.h"
#include "BillboardPipeline.h"
#include "ModuleSamplerHeap.h"
#include "ReadData.h"
#include <d3dx12.h>

bool BillboardPipeline::init(ID3D12Device* device) {
    return createRootSignature(device) && createPSO(device);
}

bool BillboardPipeline::createRootSignature(ID3D12Device* device) {
    CD3DX12_DESCRIPTOR_RANGE texRange;     texRange    .Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE samplerRange; samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
                                                             ModuleSamplerHeap::COUNT, 0);

    CD3DX12_ROOT_PARAMETER params[3];
    params[SLOT_CB     ].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);
    params[SLOT_TEXTURE].InitAsDescriptorTable(1, &texRange,     D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_SAMPLER].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(params), params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    if (FAILED(hr)) {
        if (error) OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
        LOG("BillboardPipeline: serialize root sig failed 0x%08X", hr);
        return false;
    }
    hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                      IID_PPV_ARGS(&m_rootSig));
    if (FAILED(hr)) { LOG("BillboardPipeline: CreateRootSignature failed 0x%08X", hr); return false; }
    return true;
}

bool BillboardPipeline::createPSO(ID3D12Device* device) {
    auto vs = DX::ReadData(L"BillboardVS.cso");
    auto ps = DX::ReadData(L"BillboardPS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature        = m_rootSig.Get();
    desc.VS                    = { vs.data(), vs.size() };
    desc.PS                    = { ps.data(), ps.size() };
    desc.InputLayout           = { nullptr, 0 }; // quad built in VS from SV_VertexID
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    desc.NumRenderTargets = 1;
    desc.RTVFormats[0]    = DXGI_FORMAT_R8G8B8A8_UNORM; // matches scene colour RT
    desc.DSVFormat        = DXGI_FORMAT_D32_FLOAT;      // matches GBuffer depth
    desc.SampleDesc       = { 1, 0 };
    desc.SampleMask       = UINT_MAX;

    desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    // Billboards are camera-facing quads but axial alignment can be viewed edge-on
    // from above/below — disable culling so they never vanish.
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    // Standard alpha blend: src*srcAlpha + dst*(1-srcAlpha)
    desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    auto& rt = desc.BlendState.RenderTarget[0];
    rt.BlendEnable           = TRUE;
    rt.SrcBlend              = D3D12_BLEND_SRC_ALPHA;
    rt.DestBlend             = D3D12_BLEND_INV_SRC_ALPHA;
    rt.BlendOp               = D3D12_BLEND_OP_ADD;
    rt.SrcBlendAlpha         = D3D12_BLEND_ONE;
    rt.DestBlendAlpha        = D3D12_BLEND_ZERO;
    rt.BlendOpAlpha          = D3D12_BLEND_OP_ADD;
    rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // Depth test ON, depth write OFF — share GBuffer depth read-only, same as
    // the transparent forward mesh pass.
    desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    desc.DepthStencilState.DepthEnable    = TRUE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState.DepthFunc      = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    HRESULT hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_pso));
    if (FAILED(hr)) { LOG("BillboardPipeline: CreateGraphicsPipelineState failed 0x%08X", hr); return false; }

    // Additive variant — same desc, only the blend mode differs:
    // src*srcAlpha + dst*1 (lecture: "Set additive blending for alpha").
    auto& art = desc.BlendState.RenderTarget[0];
    art.SrcBlend  = D3D12_BLEND_SRC_ALPHA;
    art.DestBlend = D3D12_BLEND_ONE;
    hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_additivePso));
    if (FAILED(hr)) { LOG("BillboardPipeline: CreateGraphicsPipelineState (additive) failed 0x%08X", hr); return false; }

    return true;
}
