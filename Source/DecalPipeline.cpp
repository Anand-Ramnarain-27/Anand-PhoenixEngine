#include "Globals.h"
#include "DecalPipeline.h"
#include "GBuffer.h"
#include "ModuleSamplerHeap.h"
#include "ReadData.h"
#include <d3dx12.h>

bool DecalPipeline::init(ID3D12Device* device){
    return createRootSignature(device) && createPSO(device);
}

bool DecalPipeline::createRootSignature(ID3D12Device* device){
    CD3DX12_DESCRIPTOR_RANGE depthRange; depthRange .Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE albedoRange; albedoRange .Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    CD3DX12_DESCRIPTOR_RANGE samplerRange; samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
                                                             ModuleSamplerHeap::COUNT, 0);

    CD3DX12_ROOT_PARAMETER params[4];
    params[SLOT_CB ].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);
    params[SLOT_DEPTH ].InitAsDescriptorTable(1, &depthRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_ALBEDO ].InitAsDescriptorTable(1, &albedoRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_SAMPLER].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(params), params, 0, nullptr,
              D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    if (FAILED(hr)){
        if (error) OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
        LOG("DecalPipeline: serialize root sig failed 0x%08X", hr);
        return false;
    }
    hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                      IID_PPV_ARGS(&m_rootSig));
    if (FAILED(hr)){ LOG("DecalPipeline: CreateRootSignature failed 0x%08X", hr); return false; }
    return true;
}

bool DecalPipeline::createPSO(ID3D12Device* device){
    auto vs = DX::ReadData(L"DecalVS.cso");
    auto ps = DX::ReadData(L"DecalPS.cso");

    D3D12_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = m_rootSig.Get();
    desc.VS = { vs.data(), vs.size() };
    desc.PS = { ps.data(), ps.size() };
    desc.InputLayout = { layout, _countof(layout) };
    desc.PrimitiveTopologyType= D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    desc.NumRenderTargets = 2;
    desc.RTVFormats[0] = GBuffer::kAlbedoFormat;
    desc.RTVFormats[1] = GBuffer::kNormalMetalRoughFormat;
    desc.DSVFormat = GBuffer::kDepthFormat;
    desc.SampleDesc = { 1, 0 };
    desc.SampleMask = UINT_MAX;

    desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    desc.BlendState.IndependentBlendEnable = TRUE;
    desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
    desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.BlendState.RenderTarget[1].RenderTargetWriteMask = 0;

    desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    HRESULT hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_pso));
    if (FAILED(hr)){ LOG("DecalPipeline: CreateGraphicsPipelineState failed 0x%08X", hr); return false; }
    return true;
}
