#include "Globals.h"
#include "LightCullingPass.h"
#include "GBufferPass.h"
#include "GBuffer.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleGPUResources.h"
#include <d3dx12.h>
#include <algorithm>

namespace {
    constexpr UINT cbAlign(UINT b){ return (b + 255u) & ~255u; }

    ComPtr<ID3D12Resource> makeUploadBuf(ID3D12Device* dev, UINT64 bytes, void** mapped, const wchar_t* name){
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer(bytes);
        ComPtr<ID3D12Resource> buf;
        HRESULT hr = dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                                                   D3D12_RESOURCE_STATE_GENERIC_READ,
                                                   nullptr, IID_PPV_ARGS(&buf));
        if (FAILED(hr)){ LOG("LightCullingPass: upload buf failed 0x%08X", hr); return nullptr; }
        buf->SetName(name);
        if (mapped) buf->Map(0, nullptr, mapped);
        return buf;
    }

    void makeStructuredSRV(ShaderTableDesc& table, UINT slot, ID3D12Resource* buf,
                            UINT numElems, UINT stride){
        D3D12_SHADER_RESOURCE_VIEW_DESC d = {};
        d.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        d.Format = DXGI_FORMAT_UNKNOWN;
        d.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        d.Buffer.NumElements = numElems;
        d.Buffer.StructureByteStride = stride;
        table.createSRV(buf, slot, &d);
    }

    void makeStructuredUAV(ShaderTableDesc& table, UINT slot, ID3D12Resource* buf,
                            UINT numElems, UINT stride){
        D3D12_UNORDERED_ACCESS_VIEW_DESC d = {};
        d.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        d.Format = DXGI_FORMAT_UNKNOWN;
        d.Buffer.NumElements = numElems;
        d.Buffer.StructureByteStride = stride;
        table.createUAV(buf, slot, &d);
    }
}

bool LightCullingPass::init(ID3D12Device* device){
    if (!m_pipeline.init(device)){
        LOG("LightCullingPass: pipeline init failed");
        return false;
    }
    if (!createUploadBuffers(device)) return false;

    LOG("LightCullingPass: init OK");
    return true;
}

bool LightCullingPass::createUploadBuffers(ID3D12Device* device){
    const UINT cbSz = cbAlign(sizeof(CbCulling));
    auto* sd = app->getShaderDescriptors();

    for (int i = 0; i < NUM_VIEWPORTS; ++i){
        m_cb[i] = makeUploadBuf(device, cbSz, &m_cbMapped[i], L"LightCulling_CB");
        if (!m_cb[i]) return false;

        m_pointLightBuf[i] = makeUploadBuf(device,
            sizeof(MeshPipeline::GPUPointLight) * MeshPipeline::MAX_POINT_LIGHTS,
            &m_pointLightMapped[i], L"LightCulling_PointLights");
        m_spotLightBuf[i] = makeUploadBuf(device,
            sizeof(MeshPipeline::GPUSpotLight) * MeshPipeline::MAX_SPOT_LIGHTS,
            &m_spotLightMapped[i], L"LightCulling_SpotLights");
        if (!m_pointLightBuf[i] || !m_spotLightBuf[i]) return false;

        m_pointLightSRV[i] = sd->allocTable("LightCulling_PointLightSRV");
        m_spotLightSRV[i] = sd->allocTable("LightCulling_SpotLightSRV");
        if (!m_pointLightSRV[i].isValid() || !m_spotLightSRV[i].isValid()){
            LOG("LightCullingPass: light SRV alloc failed");
            return false;
        }
        makeStructuredSRV(m_pointLightSRV[i], 0, m_pointLightBuf[i].Get(),
                          MeshPipeline::MAX_POINT_LIGHTS, sizeof(MeshPipeline::GPUPointLight));
        makeStructuredSRV(m_spotLightSRV[i], 0, m_spotLightBuf[i].Get(),
                          MeshPipeline::MAX_SPOT_LIGHTS, sizeof(MeshPipeline::GPUSpotLight));
    }
    return true;
}

static bool allocTileBuffers(ID3D12Device* device, UINT numTiles, UINT maxPerTile,
                              ComPtr<ID3D12Resource>& buf, const wchar_t* name){
    UINT64 bytes = (UINT64)numTiles * maxPerTile * sizeof(int);
    auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto bd = CD3DX12_RESOURCE_DESC::Buffer(bytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    ComPtr<ID3D12Resource> newBuf;
    HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                                                  D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                                  nullptr, IID_PPV_ARGS(&newBuf));
    if (FAILED(hr)){ LOG("LightCullingPass: tile buf alloc failed 0x%08X", hr); return false; }
    newBuf->SetName(name);
    buf = std::move(newBuf);
    return true;
}

void LightCullingPass::cull(ID3D12GraphicsCommandList* cmd,
                             GBufferPass& gbufferPass,
                             const FrameLightData& lights,
                             const Matrix& view,
                             const Matrix& projection,
                             uint32_t width, uint32_t height,
                             int viewportIndex){
    if (width == 0 || height == 0) return;
    viewportIndex = (viewportIndex >= 0 && viewportIndex < NUM_VIEWPORTS) ? viewportIndex : 0;

    const uint32_t tilesX = getNumTilesX(width);
    const uint32_t tilesY = getNumTilesY(height);
    const UINT numTiles = tilesX * tilesY;

    bool freshlyAllocated = false;
    if (numTiles > m_allocatedTiles[viewportIndex] || !m_pointListBuf[viewportIndex]){
        freshlyAllocated = true;
        auto* gpuRes = app->getGPUResources();
        if (m_pointListBuf[viewportIndex]) gpuRes->deferRelease(m_pointListBuf[viewportIndex]);
        if (m_spotListBuf[viewportIndex]) gpuRes->deferRelease(m_spotListBuf[viewportIndex]);

        auto* device = app->getD3D12()->getDevice();
        if (!allocTileBuffers(device, numTiles, MAX_LIGHTS_PER_TILE,
                              m_pointListBuf[viewportIndex], L"LightCulling_PointList")) return;
        if (!allocTileBuffers(device, numTiles, MAX_LIGHTS_PER_TILE,
                              m_spotListBuf[viewportIndex], L"LightCulling_SpotList")) return;
        m_allocatedTiles[viewportIndex] = numTiles;

        auto* sd = app->getShaderDescriptors();
        m_pointListUAV[viewportIndex] = sd->allocTable("LightCulling_PointUAV");
        m_spotListUAV[viewportIndex] = sd->allocTable("LightCulling_SpotUAV");
        m_pointListSRV[viewportIndex] = sd->allocTable("LightCulling_PointSRV");
        m_spotListSRV[viewportIndex] = sd->allocTable("LightCulling_SpotSRV");

        makeStructuredUAV(m_pointListUAV[viewportIndex], 0, m_pointListBuf[viewportIndex].Get(),
                          numTiles * MAX_LIGHTS_PER_TILE, sizeof(int));
        makeStructuredUAV(m_spotListUAV[viewportIndex], 0, m_spotListBuf[viewportIndex].Get(),
                          numTiles * MAX_LIGHTS_PER_TILE, sizeof(int));
        makeStructuredSRV(m_pointListSRV[viewportIndex], 0, m_pointListBuf[viewportIndex].Get(),
                          numTiles * MAX_LIGHTS_PER_TILE, sizeof(int));
        makeStructuredSRV(m_spotListSRV[viewportIndex], 0, m_spotListBuf[viewportIndex].Get(),
                          numTiles * MAX_LIGHTS_PER_TILE, sizeof(int));
    }

    {
        UINT nP = (UINT)std::min(lights.pointLights.size(), (size_t)MeshPipeline::MAX_POINT_LIGHTS);
        UINT nS = (UINT)std::min(lights.spotLights.size(), (size_t)MeshPipeline::MAX_SPOT_LIGHTS);
        if (nP) memcpy(m_pointLightMapped[viewportIndex], lights.pointLights.data(), nP * sizeof(MeshPipeline::GPUPointLight));
        if (nS) memcpy(m_spotLightMapped[viewportIndex], lights.spotLights.data(), nS * sizeof(MeshPipeline::GPUSpotLight));
    }

    {
        CbCulling cb = {};
        cb.numPointLights = (uint32_t)std::min(lights.pointLights.size(), (size_t)MeshPipeline::MAX_POINT_LIGHTS);
        cb.numSpotLights = (uint32_t)std::min(lights.spotLights.size(), (size_t)MeshPipeline::MAX_SPOT_LIGHTS);
        cb.viewportWidth = width;
        cb.viewportHeight = height;
        cb.projection = projection.Transpose();
        cb.view = view.Transpose();
        memcpy(m_cbMapped[viewportIndex], &cb, sizeof(cb));
    }

    BEGIN_EVENT(cmd, L"Light Culling Pass");

    {
        auto bar = CD3DX12_RESOURCE_BARRIER::Transition(
            gbufferPass.getGBuffer().getDepthTexture(),
            D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        cmd->ResourceBarrier(1, &bar);
    }

    {
        CD3DX12_RESOURCE_BARRIER barriers[2] = {
            CD3DX12_RESOURCE_BARRIER::Transition(m_pointListBuf[viewportIndex].Get(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
            CD3DX12_RESOURCE_BARRIER::Transition(m_spotListBuf[viewportIndex].Get(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
        };
        if (m_lastWidth[viewportIndex] != 0 && !freshlyAllocated)
            cmd->ResourceBarrier(2, barriers);
    }

    cmd->SetPipelineState(m_pipeline.getPSO());
    cmd->SetComputeRootSignature(m_pipeline.getRootSig());

    ID3D12DescriptorHeap* heaps[] = { app->getShaderDescriptors()->getHeap() };
    cmd->SetDescriptorHeaps(1, heaps);

    cmd->SetComputeRootConstantBufferView(LightCullingPipeline::SLOT_CB, m_cb[viewportIndex]->GetGPUVirtualAddress());

    cmd->SetComputeRootDescriptorTable(LightCullingPipeline::SLOT_DEPTH,
                                        gbufferPass.getGBuffer().getDepthSrvHandle());
    cmd->SetComputeRootDescriptorTable(LightCullingPipeline::SLOT_POINT_LIGHTS,
                                        m_pointLightSRV[viewportIndex].getGPUHandle(0));
    cmd->SetComputeRootDescriptorTable(LightCullingPipeline::SLOT_SPOT_LIGHTS,
                                        m_spotLightSRV[viewportIndex].getGPUHandle(0));
    cmd->SetComputeRootDescriptorTable(LightCullingPipeline::SLOT_POINT_UAV,
                                        m_pointListUAV[viewportIndex].getGPUHandle(0));
    cmd->SetComputeRootDescriptorTable(LightCullingPipeline::SLOT_SPOT_UAV,
                                        m_spotListUAV[viewportIndex].getGPUHandle(0));

    cmd->Dispatch(tilesX, tilesY, 1);

    {
        CD3DX12_RESOURCE_BARRIER barriers[3] = {
            CD3DX12_RESOURCE_BARRIER::Transition(m_pointListBuf[viewportIndex].Get(),
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
            CD3DX12_RESOURCE_BARRIER::Transition(m_spotListBuf[viewportIndex].Get(),
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
            CD3DX12_RESOURCE_BARRIER::Transition(
                gbufferPass.getGBuffer().getDepthTexture(),
                D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        };
        cmd->ResourceBarrier(3, barriers);
    }

    END_EVENT(cmd);

    m_lastWidth[viewportIndex] = width;
    m_lastHeight[viewportIndex] = height;
}
