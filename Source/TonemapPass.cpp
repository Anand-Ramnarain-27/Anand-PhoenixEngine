#include "Globals.h"
#include "TonemapPass.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleGPUResources.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplerHeap.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "RenderTexture.h"
#include "ColorLUT.h"
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
    if (!createFallbackTextures(device)){
        LOG("TonemapPass: fallback texture creation failed");
        return false;
    }
    LOG("TonemapPass: init OK");
    return true;
}

bool TonemapPass::createRootSignature(ID3D12Device* device){
    CD3DX12_DESCRIPTOR_RANGE sceneRange;
    sceneRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE bloomRange;
    bloomRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    CD3DX12_DESCRIPTOR_RANGE lutRange;
    lutRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
    CD3DX12_DESCRIPTOR_RANGE sampRange;
    sampRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, ModuleSamplerHeap::COUNT, 0);

    CD3DX12_ROOT_PARAMETER params[5];
    params[0].InitAsConstants(4, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    params[1].InitAsDescriptorTable(1, &sceneRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[2].InitAsDescriptorTable(1, &bloomRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[3].InitAsDescriptorTable(1, &lutRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[4].InitAsDescriptorTable(1, &sampRange, D3D12_SHADER_VISIBILITY_PIXEL);

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

bool TonemapPass::createFallbackTextures(ID3D12Device* device){
    uint8_t black[4] = { 0, 0, 0, 0 };
    m_fallbackBloomTex = app->getGPUResources()->createRawTexture2D(black, 4, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
    if (!m_fallbackBloomTex) return false;
    m_fallbackBloomSRV = app->getShaderDescriptors()->allocTable("TonemapFallbackBloom");
    if (!m_fallbackBloomSRV.isValid()) return false;
    m_fallbackBloomSRV.createTexture2DSRV(m_fallbackBloomTex.Get(), 0);

    float white[3] = { 1.f, 1.f, 1.f };
    D3D12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex3D(DXGI_FORMAT_R32G32B32_FLOAT, 1, 1, 1, 1);
    auto heapDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    if (FAILED(device->CreateCommittedResource(&heapDefault, D3D12_HEAP_FLAG_NONE, &texDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_fallbackLutTex))))
        return false;
    m_fallbackLutTex->SetName(L"TonemapFallbackLUT");

    UINT64 uploadSize = GetRequiredIntermediateSize(m_fallbackLutTex.Get(), 0, 1);
    auto heapUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
    ComPtr<ID3D12Resource> uploadBuf;
    if (FAILED(device->CreateCommittedResource(&heapUpload, D3D12_HEAP_FLAG_NONE, &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuf))))
        return false;

    D3D12_SUBRESOURCE_DATA sub = {};
    sub.pData = white;
    sub.RowPitch = sizeof(float) * 3;
    sub.SlicePitch = sub.RowPitch;

    ComPtr<ID3D12CommandAllocator> alloc;
    ComPtr<ID3D12GraphicsCommandList> cmd;
    if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc)))) return false;
    if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr, IID_PPV_ARGS(&cmd)))) return false;

    UpdateSubresources(cmd.Get(), m_fallbackLutTex.Get(), uploadBuf.Get(), 0, 0, 1, &sub);
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_fallbackLutTex.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmd->ResourceBarrier(1, &barrier);
    cmd->Close();

    ID3D12CommandList* lists[] = { cmd.Get() };
    app->getD3D12()->getDrawCommandQueue()->ExecuteCommandLists(1, lists);
    app->getD3D12()->flush();

    m_fallbackLutSRV = app->getShaderDescriptors()->allocTable("TonemapFallbackLUT");
    if (!m_fallbackLutSRV.isValid()) return false;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture3D.MipLevels = 1;
    m_fallbackLutSRV.createSRV(m_fallbackLutTex.Get(), 0, &srvDesc);

    return true;
}

void TonemapPass::render(ID3D12GraphicsCommandList* cmd, RenderTexture* hdrSource, RenderTexture* ldrDest,
                         RenderTexture* bloomSource, ColorLUT* lut, const TonemapParams& params){
    if (!hdrSource || !ldrDest || !hdrSource->isValid() || !ldrDest->isValid()) return;

    BEGIN_EVENT(cmd, L"Tonemap Pass");

    ldrDest->beginRender(cmd, false);

    ID3D12DescriptorHeap* heaps[] = { app->getShaderDescriptors()->getHeap(), app->getSamplerHeap()->getHeap() };
    cmd->SetDescriptorHeaps(2, heaps);

    const bool lutReady = lut && lut->isValid() && params.lutEnabled;
    float constants[4] = { params.exposure, params.bloomIntensity, lutReady ? 1.0f : 0.0f, 0.0f };

    cmd->SetGraphicsRootSignature(m_rootSig.Get());
    cmd->SetPipelineState(m_pso.Get());
    cmd->SetGraphicsRoot32BitConstants(0, 4, constants, 0);
    cmd->SetGraphicsRootDescriptorTable(1, hdrSource->getSrvHandle());
    cmd->SetGraphicsRootDescriptorTable(2, (bloomSource && bloomSource->isValid()) ? bloomSource->getSrvHandle() : m_fallbackBloomSRV.getGPUHandle(0));
    cmd->SetGraphicsRootDescriptorTable(3, lutReady ? lut->getSrvHandle() : m_fallbackLutSRV.getGPUHandle(0));
    cmd->SetGraphicsRootDescriptorTable(4, app->getSamplerHeap()->getGPUHandle(ModuleSamplerHeap::LINEAR_WRAP));

    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 0, nullptr);
    cmd->DrawInstanced(3, 1, 0, 0);

    ldrDest->endRender(cmd);

    END_EVENT(cmd);
}
