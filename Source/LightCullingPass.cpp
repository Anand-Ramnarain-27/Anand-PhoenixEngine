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
    constexpr UINT cbAlign(UINT b) { return (b + 255u) & ~255u; }

    ComPtr<ID3D12Resource> makeUploadBuf(ID3D12Device* dev, UINT64 bytes, void** mapped, const wchar_t* name) {
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer(bytes);
        ComPtr<ID3D12Resource> buf;
        HRESULT hr = dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                                                   D3D12_RESOURCE_STATE_GENERIC_READ,
                                                   nullptr, IID_PPV_ARGS(&buf));
        if (FAILED(hr)) { LOG("LightCullingPass: upload buf failed 0x%08X", hr); return nullptr; }
        buf->SetName(name);
        if (mapped) buf->Map(0, nullptr, mapped);
        return buf;
    }

    void makeStructuredSRV(ShaderTableDesc& table, UINT slot, ID3D12Resource* buf,
                            UINT numElems, UINT stride) {
        D3D12_SHADER_RESOURCE_VIEW_DESC d = {};
        d.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        d.Format = DXGI_FORMAT_UNKNOWN;
        d.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        d.Buffer.NumElements      = numElems;
        d.Buffer.StructureByteStride = stride;
        table.createSRV(buf, slot, &d);
    }

    void makeStructuredUAV(ShaderTableDesc& table, UINT slot, ID3D12Resource* buf,
                            UINT numElems, UINT stride) {
        D3D12_UNORDERED_ACCESS_VIEW_DESC d = {};
        d.ViewDimension       = D3D12_UAV_DIMENSION_BUFFER;
        d.Format              = DXGI_FORMAT_UNKNOWN;
        d.Buffer.NumElements  = numElems;
        d.Buffer.StructureByteStride = stride;
        table.createUAV(buf, slot, &d);
    }
}

bool LightCullingPass::init(ID3D12Device* device) {
    if (!m_pipeline.init(device)) {
        LOG("LightCullingPass: pipeline init failed");
        return false;
    }
    if (!createUploadBuffers(device)) return false;

    LOG("LightCullingPass: init OK");
    return true;
}

bool LightCullingPass::createUploadBuffers(ID3D12Device* device) {
    const UINT cbSz = cbAlign(sizeof(CbCulling));
    m_cb = makeUploadBuf(device, cbSz, &m_cbMapped, L"LightCulling_CB");
    if (!m_cb) return false;

    m_pointLightBuf = makeUploadBuf(device,
        sizeof(MeshPipeline::GPUPointLight) * MeshPipeline::MAX_POINT_LIGHTS,
        &m_pointLightMapped, L"LightCulling_PointLights");
    m_spotLightBuf = makeUploadBuf(device,
        sizeof(MeshPipeline::GPUSpotLight) * MeshPipeline::MAX_SPOT_LIGHTS,
        &m_spotLightMapped, L"LightCulling_SpotLights");
    if (!m_pointLightBuf || !m_spotLightBuf) return false;

    auto* sd = app->getShaderDescriptors();
    m_pointLightSRV = sd->allocTable("LightCulling_PointLightSRV");
    m_spotLightSRV  = sd->allocTable("LightCulling_SpotLightSRV");
    if (!m_pointLightSRV.isValid() || !m_spotLightSRV.isValid()) {
        LOG("LightCullingPass: light SRV alloc failed");
        return false;
    }
    makeStructuredSRV(m_pointLightSRV, 0, m_pointLightBuf.Get(),
                      MeshPipeline::MAX_POINT_LIGHTS, sizeof(MeshPipeline::GPUPointLight));
    makeStructuredSRV(m_spotLightSRV,  0, m_spotLightBuf.Get(),
                      MeshPipeline::MAX_SPOT_LIGHTS,  sizeof(MeshPipeline::GPUSpotLight));
    return true;
}

// Allocate/reallocate the per-tile index UAV buffers when the viewport size changes.
static bool allocTileBuffers(ID3D12Device* device, UINT numTiles, UINT maxPerTile,
                              ComPtr<ID3D12Resource>& buf, const wchar_t* name) {
    UINT64 bytes = (UINT64)numTiles * maxPerTile * sizeof(int);
    auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto bd = CD3DX12_RESOURCE_DESC::Buffer(bytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    ComPtr<ID3D12Resource> newBuf;
    HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                                                  D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                                  nullptr, IID_PPV_ARGS(&newBuf));
    if (FAILED(hr)) { LOG("LightCullingPass: tile buf alloc failed 0x%08X", hr); return false; }
    newBuf->SetName(name);
    buf = std::move(newBuf);
    return true;
}

void LightCullingPass::cull(ID3D12GraphicsCommandList* cmd,
                             GBufferPass& gbufferPass,
                             const FrameLightData& lights,
                             const Matrix& view,
                             const Matrix& projection,
                             uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return;

    const uint32_t tilesX = getNumTilesX(width);
    const uint32_t tilesY = getNumTilesY(height);
    const UINT numTiles = tilesX * tilesY;

    // Reallocate tile buffers if viewport changed
    if (numTiles != m_allocatedTiles || !m_pointListBuf) {
        auto* device = app->getD3D12()->getDevice();
        if (!allocTileBuffers(device, numTiles, MAX_LIGHTS_PER_TILE,
                              m_pointListBuf, L"LightCulling_PointList")) return;
        if (!allocTileBuffers(device, numTiles, MAX_LIGHTS_PER_TILE,
                              m_spotListBuf,  L"LightCulling_SpotList"))  return;
        m_allocatedTiles = numTiles;

        // Recreate descriptors for the new buffers
        auto* sd = app->getShaderDescriptors();
        m_pointListUAV = sd->allocTable("LightCulling_PointUAV");
        m_spotListUAV  = sd->allocTable("LightCulling_SpotUAV");
        m_pointListSRV = sd->allocTable("LightCulling_PointSRV");
        m_spotListSRV  = sd->allocTable("LightCulling_SpotSRV");

        makeStructuredUAV(m_pointListUAV, 0, m_pointListBuf.Get(),
                          numTiles * MAX_LIGHTS_PER_TILE, sizeof(int));
        makeStructuredUAV(m_spotListUAV,  0, m_spotListBuf.Get(),
                          numTiles * MAX_LIGHTS_PER_TILE, sizeof(int));
        makeStructuredSRV(m_pointListSRV, 0, m_pointListBuf.Get(),
                          numTiles * MAX_LIGHTS_PER_TILE, sizeof(int));
        makeStructuredSRV(m_spotListSRV,  0, m_spotListBuf.Get(),
                          numTiles * MAX_LIGHTS_PER_TILE, sizeof(int));
    }

    // Upload light data
    {
        UINT nP = (UINT)std::min(lights.pointLights.size(), (size_t)MeshPipeline::MAX_POINT_LIGHTS);
        UINT nS = (UINT)std::min(lights.spotLights.size(),  (size_t)MeshPipeline::MAX_SPOT_LIGHTS);
        if (nP) memcpy(m_pointLightMapped, lights.pointLights.data(), nP * sizeof(MeshPipeline::GPUPointLight));
        if (nS) memcpy(m_spotLightMapped,  lights.spotLights.data(),  nS * sizeof(MeshPipeline::GPUSpotLight));
    }

    // Upload CB
    {
        CbCulling cb = {};
        cb.numPointLights = (uint32_t)std::min(lights.pointLights.size(), (size_t)MeshPipeline::MAX_POINT_LIGHTS);
        cb.numSpotLights  = (uint32_t)std::min(lights.spotLights.size(),  (size_t)MeshPipeline::MAX_SPOT_LIGHTS);
        cb.viewportWidth  = width;
        cb.viewportHeight = height;
        cb.projection     = projection.Transpose();
        cb.view           = view.Transpose();
        memcpy(m_cbMapped, &cb, sizeof(cb));
    }

    BEGIN_EVENT(cmd, L"Light Culling Pass");

    // Transition depth: DEPTH_READ|PSR → DEPTH_READ|PSR|NON_PSR so the compute shader can read it
    {
        auto bar = CD3DX12_RESOURCE_BARRIER::Transition(
            gbufferPass.getGBuffer().getDepthTexture(),
            D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        cmd->ResourceBarrier(1, &bar);
    }

    // Transition tile buffers PSR → UAV for compute write
    // (On first dispatch after alloc they are already in UAV state, but the transition is a no-op.)
    {
        CD3DX12_RESOURCE_BARRIER barriers[2] = {
            CD3DX12_RESOURCE_BARRIER::Transition(m_pointListBuf.Get(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
            CD3DX12_RESOURCE_BARRIER::Transition(m_spotListBuf.Get(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
        };
        // Only issue these if not first frame (buffers start in UAV after alloc)
        if (m_lastWidth != 0)
            cmd->ResourceBarrier(2, barriers);
    }

    cmd->SetPipelineState(m_pipeline.getPSO());
    cmd->SetComputeRootSignature(m_pipeline.getRootSig());

    ID3D12DescriptorHeap* heaps[] = { app->getShaderDescriptors()->getHeap() };
    cmd->SetDescriptorHeaps(1, heaps);

    cmd->SetComputeRootConstantBufferView(LightCullingPipeline::SLOT_CB, m_cb->GetGPUVirtualAddress());

    // Depth SRV (already in PIXEL_SHADER_RESOURCE after endGeomPass)
    cmd->SetComputeRootDescriptorTable(LightCullingPipeline::SLOT_DEPTH,
                                        gbufferPass.getGBuffer().getDepthSrvHandle());
    cmd->SetComputeRootDescriptorTable(LightCullingPipeline::SLOT_POINT_LIGHTS,
                                        m_pointLightSRV.getGPUHandle(0));
    cmd->SetComputeRootDescriptorTable(LightCullingPipeline::SLOT_SPOT_LIGHTS,
                                        m_spotLightSRV.getGPUHandle(0));
    cmd->SetComputeRootDescriptorTable(LightCullingPipeline::SLOT_POINT_UAV,
                                        m_pointListUAV.getGPUHandle(0));
    cmd->SetComputeRootDescriptorTable(LightCullingPipeline::SLOT_SPOT_UAV,
                                        m_spotListUAV.getGPUHandle(0));

    cmd->Dispatch(tilesX, tilesY, 1);

    // Transition tile buffers from UAV to SRV so the lighting pass can read them
    // Also restore depth to DEPTH_READ|PSR (remove NON_PSR)
    {
        CD3DX12_RESOURCE_BARRIER barriers[3] = {
            CD3DX12_RESOURCE_BARRIER::Transition(m_pointListBuf.Get(),
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
            CD3DX12_RESOURCE_BARRIER::Transition(m_spotListBuf.Get(),
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

    m_lastWidth  = width;
    m_lastHeight = height;
}
