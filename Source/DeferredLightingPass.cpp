#include "Globals.h"
#include "DeferredLightingPass.h"
#include "GBufferPass.h"
#include "GBuffer.h"
#include "EnvironmentSystem.h"
#include "EnvironmentMap.h"
#include "MeshRenderPass.h"
#include "ModuleSamplerHeap.h"
#include "ModuleShaderDescriptors.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include <d3dx12.h>
#include <algorithm>

namespace {
    constexpr UINT cbAlign(UINT b) { return (b + 255u) & ~255u; }

    ComPtr<ID3D12Resource> makeUploadBuf(ID3D12Device* device, UINT64 bytes,
                                          void** mapped, const wchar_t* name) {
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer(bytes);
        ComPtr<ID3D12Resource> buf;
        HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                                                      D3D12_RESOURCE_STATE_GENERIC_READ,
                                                      nullptr, IID_PPV_ARGS(&buf));
        if (FAILED(hr)) { LOG("DeferredLightingPass: buf create failed 0x%08X", hr); return nullptr; }
        buf->SetName(name);
        if (mapped) buf->Map(0, nullptr, mapped);
        return buf;
    }

    void makeStructuredSRV(ShaderTableDesc& table, UINT slot,
                            ID3D12Resource* buf, UINT numElems, UINT stride) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
        srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv.Format = DXGI_FORMAT_UNKNOWN;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Buffer.NumElements = numElems;
        srv.Buffer.StructureByteStride = stride;
        table.createSRV(buf, slot, &srv);
    }

    void writeFallbackCubeSRV(ShaderTableDesc& table, UINT slot, ID3D12Resource* cube) {
        D3D12_SHADER_RESOURCE_VIEW_DESC sv = {};
        sv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sv.TextureCube.MipLevels = 1;
        sv.TextureCube.MostDetailedMip = 0;
        sv.TextureCube.ResourceMinLODClamp = 0.f;
        table.createSRV(cube, slot, &sv);
    }

    void writeFallbackTex2DSRV(ShaderTableDesc& table, UINT slot, ID3D12Resource* tex) {
        D3D12_SHADER_RESOURCE_VIEW_DESC sv = {};
        sv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sv.Texture2D.MipLevels = 1;
        table.createSRV(tex, slot, &sv);
    }
}

bool DeferredLightingPass::init(ID3D12Device* device) {
    if (!m_pipeline.init(device)) {
        LOG("DeferredLightingPass: pipeline init failed");
        return false;
    }
    if (!createUploadBuffers(device)) return false;
    if (!createLightSRVs()) return false;
    if (!createFallbackIBL(device)) return false;
    LOG("DeferredLightingPass: init OK");
    return true;
}

bool DeferredLightingPass::createUploadBuffers(ID3D12Device* device) {
    const UINT cbSz = cbAlign(sizeof(CbPerFrame));
    m_perFrameCB = makeUploadBuf(device, cbSz, &m_perFrameMapped, L"DeferredLight_PerFrameCB");
    if (!m_perFrameCB) return false;

    m_dirLightBuf = makeUploadBuf(device,
        sizeof(MeshPipeline::GPUDirectionalLight) * MeshPipeline::MAX_DIR_LIGHTS,
        &m_dirLightMapped, L"DeferredLight_DirLights");
    m_pointLightBuf = makeUploadBuf(device,
        sizeof(MeshPipeline::GPUPointLight) * MeshPipeline::MAX_POINT_LIGHTS,
        &m_pointLightMapped, L"DeferredLight_PointLights");
    m_spotLightBuf = makeUploadBuf(device,
        sizeof(MeshPipeline::GPUSpotLight) * MeshPipeline::MAX_SPOT_LIGHTS,
        &m_spotLightMapped, L"DeferredLight_SpotLights");

    if (!m_dirLightBuf || !m_pointLightBuf || !m_spotLightBuf) return false;
    return true;
}

bool DeferredLightingPass::createLightSRVs() {
    auto* sd = app->getShaderDescriptors();
    m_dirLightSRV = sd->allocTable("DeferredLight_DirSRV");
    m_pointLightSRV = sd->allocTable("DeferredLight_PointSRV");
    m_spotLightSRV = sd->allocTable("DeferredLight_SpotSRV");
    if (!m_dirLightSRV.isValid() || !m_pointLightSRV.isValid() || !m_spotLightSRV.isValid()) {
        LOG("DeferredLightingPass: light SRV alloc failed");
        return false;
    }
    makeStructuredSRV(m_dirLightSRV, 0, m_dirLightBuf.Get(),
                       MeshPipeline::MAX_DIR_LIGHTS, sizeof(MeshPipeline::GPUDirectionalLight));
    makeStructuredSRV(m_pointLightSRV, 0, m_pointLightBuf.Get(),
                       MeshPipeline::MAX_POINT_LIGHTS, sizeof(MeshPipeline::GPUPointLight));
    makeStructuredSRV(m_spotLightSRV, 0, m_spotLightBuf.Get(),
                       MeshPipeline::MAX_SPOT_LIGHTS, sizeof(MeshPipeline::GPUSpotLight));
    return true;
}

bool DeferredLightingPass::createFallbackIBL(ID3D12Device* device) {
    {
        D3D12_RESOURCE_DESC td = {};
        td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        td.Width = td.Height = 1;
        td.DepthOrArraySize = 6;
        td.MipLevels = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc = { 1, 0 };
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td,
                                                      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                                      nullptr, IID_PPV_ARGS(&m_fallbackCube));
        if (FAILED(hr)) {
            LOG("DeferredLightingPass: fallback cube failed 0x%08X", hr);
            return false;
        }
        m_fallbackCube->SetName(L"DeferredLight_FallbackCube");
    }
    {
        D3D12_RESOURCE_DESC td = {};
        td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        td.Width = td.Height = 1;
        td.DepthOrArraySize = 1;
        td.MipLevels = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc = { 1, 0 };
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td,
                                                      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                                      nullptr, IID_PPV_ARGS(&m_fallbackTex2D));
        if (FAILED(hr)) {
            LOG("DeferredLightingPass: fallback 2D failed 0x%08X", hr);
            return false;
        }
        m_fallbackTex2D->SetName(L"DeferredLight_FallbackTex2D");
    }

    auto* sd = app->getShaderDescriptors();
    m_fallbackIrradianceSRV = sd->allocTable("DeferredLight_FallbackIrr");
    m_fallbackPrefilterSRV = sd->allocTable("DeferredLight_FallbackPref");
    m_fallbackBRDFSRV = sd->allocTable("DeferredLight_FallbackBRDF");
    if (!m_fallbackIrradianceSRV.isValid() || !m_fallbackPrefilterSRV.isValid()
                                           || !m_fallbackBRDFSRV.isValid()) {
        LOG("DeferredLightingPass: fallback IBL SRV alloc failed");
        return false;
    }
    writeFallbackCubeSRV(m_fallbackIrradianceSRV, 0, m_fallbackCube.Get());
    writeFallbackCubeSRV(m_fallbackPrefilterSRV, 0, m_fallbackCube.Get());
    writeFallbackTex2DSRV(m_fallbackBRDFSRV, 0, m_fallbackTex2D.Get());
    return true;
}

void DeferredLightingPass::uploadLights(const FrameLightData& lights) {
    auto copy = [](void* dst, const void* src, size_t count, size_t stride, size_t maxCount) {
        UINT n = static_cast<UINT>(std::min(count, maxCount));
        if (n > 0) memcpy(dst, src, n * stride);
    };
    copy(m_dirLightMapped, lights.dirLights.data(), lights.dirLights.size(),
         sizeof(MeshPipeline::GPUDirectionalLight), MeshPipeline::MAX_DIR_LIGHTS);
    copy(m_pointLightMapped, lights.pointLights.data(), lights.pointLights.size(),
         sizeof(MeshPipeline::GPUPointLight), MeshPipeline::MAX_POINT_LIGHTS);
    copy(m_spotLightMapped, lights.spotLights.data(), lights.spotLights.size(),
         sizeof(MeshPipeline::GPUSpotLight), MeshPipeline::MAX_SPOT_LIGHTS);
}

void DeferredLightingPass::uploadPerFrameCB(const FrameLightData& lights,
                                             const Vector3& cameraPos,
                                             const Matrix& invViewProj,
                                             uint32_t envRoughLevels) {
    CbPerFrame cb = {};
    cb.dirLightCount = static_cast<uint32_t>(std::min(lights.dirLights.size(), (size_t)MeshPipeline::MAX_DIR_LIGHTS));
    cb.pointLightCount = static_cast<uint32_t>(std::min(lights.pointLights.size(), (size_t)MeshPipeline::MAX_POINT_LIGHTS));
    cb.spotLightCount = static_cast<uint32_t>(std::min(lights.spotLights.size(), (size_t)MeshPipeline::MAX_SPOT_LIGHTS));
    cb.envRoughnessLevels = envRoughLevels;
    cb.cameraPosition = cameraPos;
    cb.framePad = 0;
    cb.invViewProj = invViewProj.Transpose();
    memcpy(m_perFrameMapped, &cb, sizeof(cb));
}

void DeferredLightingPass::render(ID3D12GraphicsCommandList* cmd,
                                   GBufferPass& gbufferPass,
                                   const FrameLightData& lights,
                                   const Vector3& cameraPos,
                                   const Matrix& invViewProj,
                                   const EnvironmentSystem* env,
                                   uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return;
    if (!gbufferPass.getGBuffer().isValid()) return;

    uint32_t roughLevels = 0;
    if (env && env->hasIBL()) roughLevels = EnvironmentMap::NUM_ROUGHNESS_LEVELS;

    uploadLights(lights);
    uploadPerFrameCB(lights, cameraPos, invViewProj, roughLevels);

    BEGIN_EVENT(cmd, L"Deferred Lighting Pass");

    cmd->SetPipelineState(m_pipeline.getPSO());
    cmd->SetGraphicsRootSignature(m_pipeline.getRootSig());

    auto* samplerHeap = app->getSamplerHeap();
    ID3D12DescriptorHeap* heaps[] = {
        app->getShaderDescriptors()->getHeap(),
        samplerHeap->getHeap()
    };
    cmd->SetDescriptorHeaps(2, heaps);

    cmd->SetGraphicsRootConstantBufferView(DeferredLightingPipeline::SLOT_PERFRAME_CB,
                                            m_perFrameCB->GetGPUVirtualAddress());

    cmd->SetGraphicsRootDescriptorTable(DeferredLightingPipeline::SLOT_DIR_LIGHTS,
                                         m_dirLightSRV.getGPUHandle(0));
    cmd->SetGraphicsRootDescriptorTable(DeferredLightingPipeline::SLOT_POINT_LIGHTS,
                                         m_pointLightSRV.getGPUHandle(0));
    cmd->SetGraphicsRootDescriptorTable(DeferredLightingPipeline::SLOT_SPOT_LIGHTS,
                                         m_spotLightSRV.getGPUHandle(0));

    if (env && env->hasIBL()) {
        const EnvironmentMap* map = env->getEnvironmentMap();
        cmd->SetGraphicsRootDescriptorTable(DeferredLightingPipeline::SLOT_IRRADIANCE,
                                             map->getIrradianceGPU());
        cmd->SetGraphicsRootDescriptorTable(DeferredLightingPipeline::SLOT_PREFILTER,
                                             map->getPrefilteredGPU());
        cmd->SetGraphicsRootDescriptorTable(DeferredLightingPipeline::SLOT_BRDF_LUT,
                                             map->getBRDFLUTGPU());
    } else {
        cmd->SetGraphicsRootDescriptorTable(DeferredLightingPipeline::SLOT_IRRADIANCE,
                                             m_fallbackIrradianceSRV.getGPUHandle(0));
        cmd->SetGraphicsRootDescriptorTable(DeferredLightingPipeline::SLOT_PREFILTER,
                                             m_fallbackPrefilterSRV.getGPUHandle(0));
        cmd->SetGraphicsRootDescriptorTable(DeferredLightingPipeline::SLOT_BRDF_LUT,
                                             m_fallbackBRDFSRV.getGPUHandle(0));
    }

    GBuffer& gb = gbufferPass.getGBuffer();
    cmd->SetGraphicsRootDescriptorTable(DeferredLightingPipeline::SLOT_GBUF_ALBEDO,
                                         gb.getSrvHandle(GBuffer::Albedo));
    cmd->SetGraphicsRootDescriptorTable(DeferredLightingPipeline::SLOT_GBUF_NORMAL,
                                         gb.getSrvHandle(GBuffer::NormalMetalRough));
    cmd->SetGraphicsRootDescriptorTable(DeferredLightingPipeline::SLOT_GBUF_EMISSIVE,
                                         gb.getSrvHandle(GBuffer::EmissiveAO));
    cmd->SetGraphicsRootDescriptorTable(DeferredLightingPipeline::SLOT_GBUF_DEPTH,
                                         gb.getDepthSrvHandle());

    cmd->SetGraphicsRootDescriptorTable(DeferredLightingPipeline::SLOT_SAMPLER,
                                         samplerHeap->getGPUHandle(ModuleSamplerHeap::LINEAR_WRAP));

    D3D12_VIEWPORT vp = { 0.f, 0.f, float(width), float(height), 0.f, 1.f };
    D3D12_RECT sc = { 0, 0, LONG(width), LONG(height) };
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);

    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->DrawInstanced(3, 1, 0, 0);

    END_EVENT(cmd);
}
