#include "Globals.h"
#include "GBufferPipeline.h"
#include "GBuffer.h"
#include "Mesh.h"
#include "ModuleSamplerHeap.h"
#include "ReadData.h"
#include <d3dx12.h>

bool GBufferPipeline::init(ID3D12Device* device) {
    return createRootSignature(device) && createPSO(device);
}

bool GBufferPipeline::createRootSignature(ID3D12Device* device) {
    // 5 material textures: t0..t4
    CD3DX12_DESCRIPTOR_RANGE matRange;
    matRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0);

    // Samplers: s0..s(COUNT-1)
    CD3DX12_DESCRIPTOR_RANGE samplerRange;
    samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, ModuleSamplerHeap::COUNT, 0);

    CD3DX12_ROOT_PARAMETER params[4];
    params[SLOT_MVP_CB].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
    params[SLOT_INSTANCE_CB].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL);
    params[SLOT_MAT_TEXTURES].InitAsDescriptorTable(1, &matRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_SAMPLER].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(params), params, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    if (FAILED(hr)) {
        if (error) OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
        LOG("GBufferPipeline: D3D12SerializeRootSignature failed 0x%08X", hr);
        return false;
    }

    hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                      IID_PPV_ARGS(&m_rootSig));
    if (FAILED(hr)) {
        LOG("GBufferPipeline: CreateRootSignature failed 0x%08X", hr);
        return false;
    }
    return true;
}

bool GBufferPipeline::createPSO(ID3D12Device* device) {
    auto vs = DX::ReadData(L"GBufferVS.cso");
    auto ps = DX::ReadData(L"GBufferPS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = m_rootSig.Get();
    desc.InputLayout = { Mesh::InputLayout, Mesh::InputLayoutCount };
    desc.VS = { vs.data(), vs.size() };
    desc.PS = { ps.data(), ps.size() };
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    // 3 MRT outputs
    desc.NumRenderTargets = GBuffer::NUM_COLOR_RTS;
    desc.RTVFormats[0] = GBuffer::kAlbedoFormat;
    desc.RTVFormats[1] = GBuffer::kNormalMetalRoughFormat;
    desc.RTVFormats[2] = GBuffer::kEmissiveAOFormat;
    desc.DSVFormat = GBuffer::kDepthFormat;

    desc.SampleDesc = { 1, 0 };
    desc.SampleMask = UINT_MAX;

    desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    desc.RasterizerState.FrontCounterClockwise = TRUE;

    // Blending explicitly disabled on all MRT outputs
    desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    for (int i = 0; i < GBuffer::NUM_COLOR_RTS; ++i) {
        desc.BlendState.RenderTarget[i].BlendEnable = FALSE;
        desc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }

    desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    HRESULT hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_pso));
    if (FAILED(hr)) {
        LOG("GBufferPipeline: CreateGraphicsPipelineState failed 0x%08X", hr);
        return false;
    }
    return true;
}
