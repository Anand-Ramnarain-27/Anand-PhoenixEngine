#include "Globals.h"
#include "DeferredLightingPipeline.h"
#include "ModuleSamplerHeap.h"
#include "ReadData.h"
#include <d3dx12.h>

bool DeferredLightingPipeline::init(ID3D12Device* device) {
    return createRootSignature(device) && createPSO(device);
}

bool DeferredLightingPipeline::createRootSignature(ID3D12Device* device) {
    CD3DX12_DESCRIPTOR_RANGE dirRange;     dirRange.Init    (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE pointRange;   pointRange.Init  (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    CD3DX12_DESCRIPTOR_RANGE spotRange;    spotRange.Init   (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
    CD3DX12_DESCRIPTOR_RANGE irrRange;     irrRange.Init    (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
    CD3DX12_DESCRIPTOR_RANGE prefRange;    prefRange.Init   (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);
    CD3DX12_DESCRIPTOR_RANGE brdfRange;    brdfRange.Init   (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5);
    CD3DX12_DESCRIPTOR_RANGE albRange;     albRange.Init    (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6);
    CD3DX12_DESCRIPTOR_RANGE normRange;    normRange.Init   (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 7);
    CD3DX12_DESCRIPTOR_RANGE emissRange;   emissRange.Init  (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 8);
    CD3DX12_DESCRIPTOR_RANGE depthRange;   depthRange.Init  (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 9);
    CD3DX12_DESCRIPTOR_RANGE samplerRange; samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, ModuleSamplerHeap::COUNT, 0);

    CD3DX12_ROOT_PARAMETER params[12];
    params[SLOT_PERFRAME_CB  ].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_DIR_LIGHTS   ].InitAsDescriptorTable(1, &dirRange,     D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_POINT_LIGHTS ].InitAsDescriptorTable(1, &pointRange,   D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_SPOT_LIGHTS  ].InitAsDescriptorTable(1, &spotRange,    D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_IRRADIANCE   ].InitAsDescriptorTable(1, &irrRange,     D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_PREFILTER    ].InitAsDescriptorTable(1, &prefRange,    D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_BRDF_LUT     ].InitAsDescriptorTable(1, &brdfRange,    D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_GBUF_ALBEDO  ].InitAsDescriptorTable(1, &albRange,     D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_GBUF_NORMAL  ].InitAsDescriptorTable(1, &normRange,    D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_GBUF_EMISSIVE].InitAsDescriptorTable(1, &emissRange,   D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_GBUF_DEPTH   ].InitAsDescriptorTable(1, &depthRange,   D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_SAMPLER      ].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(params), params, 0, nullptr,
              D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    if (FAILED(hr)) {
        if (error) OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
        LOG("DeferredLightingPipeline: serialize root sig failed 0x%08X", hr);
        return false;
    }
    hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                      IID_PPV_ARGS(&m_rootSig));
    if (FAILED(hr)) {
        LOG("DeferredLightingPipeline: CreateRootSignature failed 0x%08X", hr);
        return false;
    }
    return true;
}

bool DeferredLightingPipeline::createPSO(ID3D12Device* device) {
    auto vs = DX::ReadData(L"DeferredLightingVS.cso");
    auto ps = DX::ReadData(L"DeferredLightingPS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature        = m_rootSig.Get();
    desc.InputLayout           = { nullptr, 0 };
    desc.VS                    = { vs.data(), vs.size() };
    desc.PS                    = { ps.data(), ps.size() };
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets      = 1;
    desc.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.DSVFormat             = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc            = { 1, 0 };
    desc.SampleMask            = UINT_MAX;

    desc.RasterizerState                  = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    desc.RasterizerState.CullMode         = D3D12_CULL_MODE_NONE;

    desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    desc.DepthStencilState                = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    desc.DepthStencilState.DepthEnable    = FALSE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    HRESULT hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_pso));
    if (FAILED(hr)) {
        LOG("DeferredLightingPipeline: CreateGraphicsPipelineState failed 0x%08X", hr);
        return false;
    }
    return true;
}
