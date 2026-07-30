#include "Globals.h"
#include "TonemapPass.h"
#include "Application.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplerHeap.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "RenderTexture.h"
#include "ReadData.h"
#include <d3dx12.h>

bool TonemapPass::init(ID3D12Device* device){
    if (!createRootSignature(device)){
        LOG("TonemapPass: root signature creation failed");
        return false;
    }
    if (!createPSO(device)){
        LOG("TonemapPass: PSO creation failed");
        return false;
    }
    LOG("TonemapPass: init OK");
    return true;
}

bool TonemapPass::createRootSignature(ID3D12Device* device){
    CD3DX12_DESCRIPTOR_RANGE srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_DESCRIPTOR_RANGE sampRange;
    sampRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, ModuleSamplerHeap::COUNT, 0);

    CD3DX12_ROOT_PARAMETER params[2];
    params[0].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[1].InitAsDescriptorTable(1, &sampRange, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
    rsDesc.Init(_countof(params), params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ComPtr<ID3DBlob> blob, err;
    if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err))){
        LOG("TonemapPass: root signature serialise failed: %s", err ? (char*)err->GetBufferPointer() : "unknown error");
        return false;
    }

    return SUCCEEDED(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&m_rootSig)));
}

bool TonemapPass::createPSO(ID3D12Device* device){
    auto vs = DX::ReadData(L"FullScreenVS.cso");
    auto ps = DX::ReadData(L"TonemapPS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = m_rootSig.Get();
    desc.VS = { vs.data(), vs.size() };
    desc.PS = { ps.data(), ps.size() };
    desc.InputLayout = { nullptr, 0 };
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.NumRenderTargets = 1;
    desc.SampleDesc = { 1, 0 };
    desc.SampleMask = UINT_MAX;
    desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    return SUCCEEDED(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_pso)));
}

void TonemapPass::render(ID3D12GraphicsCommandList* cmd, RenderTexture* hdrSource, RenderTexture* ldrDest){
    if (!hdrSource || !ldrDest || !hdrSource->isValid() || !ldrDest->isValid()) return;

    BEGIN_EVENT(cmd, L"Tonemap Pass");

    ldrDest->beginRender(cmd, false);

    ID3D12DescriptorHeap* heaps[] = { app->getShaderDescriptors()->getHeap(), app->getSamplerHeap()->getHeap() };
    cmd->SetDescriptorHeaps(2, heaps);

    cmd->SetGraphicsRootSignature(m_rootSig.Get());
    cmd->SetPipelineState(m_pso.Get());
    cmd->SetGraphicsRootDescriptorTable(0, hdrSource->getSrvHandle());
    cmd->SetGraphicsRootDescriptorTable(1, app->getSamplerHeap()->getGPUHandle(ModuleSamplerHeap::LINEAR_CLAMP));

    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 0, nullptr);
    cmd->DrawInstanced(3, 1, 0, 0);

    ldrDest->endRender(cmd);

    END_EVENT(cmd);
}
