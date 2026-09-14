#include "Globals.h"
#include "FogPass.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "GBufferPass.h"
#include "GBuffer.h"
#include "ModuleSamplerHeap.h"
#include "ModuleShaderDescriptors.h"
#include "Application.h"
#include "RenderTexture.h"
#include "ReadData.h"
#include <d3dx12.h>

namespace {
    constexpr UINT cbAlign(UINT b){ return (b + 255u) & ~255u; }
}

bool FogPass::init(ID3D12Device* device){
    if (!createRootSignature(device)){
        LOG("FogPass: root signature creation failed");
        return false;
    }
    if (!createPSO(device)){
        LOG("FogPass: PSO creation failed");
        return false;
    }
    if (!createUploadBuffers(device)){
        LOG("FogPass: upload buffer creation failed");
        return false;
    }
    LOG("FogPass: init OK");
    return true;
}

bool FogPass::createRootSignature(ID3D12Device* device){
    CD3DX12_DESCRIPTOR_RANGE sceneColorRange;
    sceneColorRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_DESCRIPTOR_RANGE depthRange;
    depthRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

    CD3DX12_DESCRIPTOR_RANGE sampRange;
    sampRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, ModuleSamplerHeap::COUNT, 0);

    CD3DX12_ROOT_PARAMETER params[4];
    params[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    params[1].InitAsDescriptorTable(1, &sceneColorRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[2].InitAsDescriptorTable(1, &depthRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[3].InitAsDescriptorTable(1, &sampRange, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
    rsDesc.Init(_countof(params), params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ComPtr<ID3DBlob> blob, err;
    if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err))){
        LOG("FogPass: root signature serialise failed: %s", err ? (char*)err->GetBufferPointer() : "unknown error");
        return false;
    }

    return SUCCEEDED(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&m_rootSig)));
}

bool FogPass::createPSO(ID3D12Device* device){
    auto vs = DX::ReadData(L"FullScreenVS.cso");
    auto ps = DX::ReadData(L"FogPS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = m_rootSig.Get();
    desc.VS = { vs.data(), vs.size() };
    desc.PS = { ps.data(), ps.size() };
    desc.InputLayout = { nullptr, 0 };
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.RTVFormats[0] = kSceneColorFormat;
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

bool FogPass::createUploadBuffers(ID3D12Device* device){
    const UINT cbSz = cbAlign(sizeof(CbPerFrame));
    for (int i = 0; i < NUM_VIEWPORTS; ++i){
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer(cbSz);
        HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                                                      D3D12_RESOURCE_STATE_GENERIC_READ,
                                                      nullptr, IID_PPV_ARGS(&m_perFrameCB[i]));
        if (FAILED(hr)){
            LOG("FogPass: CB create failed 0x%08X", hr);
            return false;
        }
        m_perFrameCB[i]->SetName(L"Fog_PerFrameCB");
        m_perFrameCB[i]->Map(0, nullptr, &m_perFrameMapped[i]);
    }
    return true;
}

RenderTexture* FogPass::render(ID3D12GraphicsCommandList* cmd,
                               RenderTexture* input,
                               RenderTexture* output,
                               GBufferPass& gbufferPass,
                               const Vector3& cameraPos,
                               const Matrix& invViewProj,
                               const FogSettings& settings,
                               int viewportIndex){
    if (!settings.enabled) return input;
    if (!input || !input->isValid() || !output || !output->isValid()) return input;
    if (!gbufferPass.getGBuffer().isValid()) return input;

    viewportIndex = (viewportIndex >= 0 && viewportIndex < NUM_VIEWPORTS) ? viewportIndex : 0;

    CbPerFrame cb = {};
    cb.invViewProj = invViewProj.Transpose();
    cb.cameraPosition = cameraPos;
    cb.fogColor = settings.color;
    cb.startDistance = settings.startDistance;
    cb.endDistance = settings.endDistance;
    cb.maxOpacity = settings.maxOpacity;
    memcpy(m_perFrameMapped[viewportIndex], &cb, sizeof(cb));

    BEGIN_EVENT(cmd, L"Fog");

    output->beginRender(cmd, false);

    ID3D12DescriptorHeap* heaps[] = { app->getShaderDescriptors()->getHeap(), app->getSamplerHeap()->getHeap() };
    cmd->SetDescriptorHeaps(2, heaps);

    cmd->SetGraphicsRootSignature(m_rootSig.Get());
    cmd->SetPipelineState(m_pso.Get());

    cmd->SetGraphicsRootConstantBufferView(0, m_perFrameCB[viewportIndex]->GetGPUVirtualAddress());
    cmd->SetGraphicsRootDescriptorTable(1, input->getSrvHandle());
    cmd->SetGraphicsRootDescriptorTable(2, gbufferPass.getGBuffer().getDepthSrvHandle());
    cmd->SetGraphicsRootDescriptorTable(3, app->getSamplerHeap()->getGPUHandle(ModuleSamplerHeap::LINEAR_WRAP));

    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 0, nullptr);
    cmd->DrawInstanced(3, 1, 0, 0);

    output->endRender(cmd);

    END_EVENT(cmd);

    return output;
}
