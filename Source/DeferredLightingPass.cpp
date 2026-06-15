#include "Globals.h"
#include "DeferredLightingPass.h"
#include "GBufferPass.h"
#include "GBuffer.h"
#include "EnvironmentSystem.h"
#include "EnvironmentMap.h"
#include "ForwardMeshPass.h"
#include "ModuleSamplerHeap.h"
#include "ModuleShaderDescriptors.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include <d3dx12.h>
#include <algorithm>

namespace {
    constexpr UINT cbAlign(UINT b){ return (b + 255u) & ~255u; }

    ComPtr<ID3D12Resource> makeUploadBuf(ID3D12Device* device, UINT64 bytes,
                                          void** mapped, const wchar_t* name){
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer(bytes);
        ComPtr<ID3D12Resource> buf;
        HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                                                      D3D12_RESOURCE_STATE_GENERIC_READ,
                                                      nullptr, IID_PPV_ARGS(&buf));
        if (FAILED(hr)){ LOG("DeferredLightingPass: buf create failed 0x%08X", hr); return nullptr; }
        buf->SetName(name);
        if (mapped) buf->Map(0, nullptr, mapped);
        return buf;
    }

    void makeStructuredSRV(ShaderTableDesc& table, UINT slot,
                            ID3D12Resource* buf, UINT numElems, UINT stride){
        D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
        srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv.Format = DXGI_FORMAT_UNKNOWN;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Buffer.NumElements = numElems;
        srv.Buffer.StructureByteStride = stride;
        table.createSRV(buf, slot, &srv);
    }

    void writeFallbackCubeSRV(ShaderTableDesc& table, UINT slot, ID3D12Resource* cube){
        D3D12_SHADER_RESOURCE_VIEW_DESC sv = {};
        sv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sv.TextureCube.MipLevels = 1;
        sv.TextureCube.MostDetailedMip = 0;
        sv.TextureCube.ResourceMinLODClamp = 0.f;
        table.createSRV(cube, slot, &sv);
    }

    void writeFallbackTex2DSRV(ShaderTableDesc& table, UINT slot, ID3D12Resource* tex){
        D3D12_SHADER_RESOURCE_VIEW_DESC sv = {};
        sv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sv.Texture2D.MipLevels = 1;
        table.createSRV(tex, slot, &sv);
    }
}

bool DeferredLightingPass::init(ID3D12Device* device){
    if (!m_pipeline.init(device)){
        LOG("DeferredLightingPass: pipeline init failed");
        return false;
    }
    if (!m_lightCulling.init(device)){
        LOG("DeferredLightingPass: light culling init failed");
        return false;
    }
    if (!createUploadBuffers(device)) return false;
    if (!createLightSRVs()) return false;
    if (!createFallbackIBL(device)) return false;
    LOG("DeferredLightingPass: init OK");
    return true;
}

bool DeferredLightingPass::createUploadBuffers(ID3D12Device* device){
    const UINT cbSz = cbAlign(sizeof(CbPerFrame));
    for (int i = 0; i < NUM_VIEWPORTS; ++i){
        m_perFrameCB[i] = makeUploadBuf(device, cbSz, &m_perFrameMapped[i], L"DeferredLight_PerFrameCB");
        if (!m_perFrameCB[i]) return false;

        m_dirLightBuf[i] = makeUploadBuf(device,
            sizeof(MeshPipeline::GPUDirectionalLight) * MeshPipeline::MAX_DIR_LIGHTS,
            &m_dirLightMapped[i], L"DeferredLight_DirLights");
        m_pointLightBuf[i] = makeUploadBuf(device,
            sizeof(MeshPipeline::GPUPointLight) * MeshPipeline::MAX_POINT_LIGHTS,
            &m_pointLightMapped[i], L"DeferredLight_PointLights");
        m_spotLightBuf[i] = makeUploadBuf(device,
            sizeof(MeshPipeline::GPUSpotLight) * MeshPipeline::MAX_SPOT_LIGHTS,
            &m_spotLightMapped[i], L"DeferredLight_SpotLights");

        if (!m_dirLightBuf[i] || !m_pointLightBuf[i] || !m_spotLightBuf[i]) return false;
    }
    return true;
}

bool DeferredLightingPass::createLightSRVs(){
    auto* sd = app->getShaderDescriptors();
    for (int i = 0; i < NUM_VIEWPORTS; ++i){
        m_dirLightSRV[i] = sd->allocTable("DeferredLight_DirSRV");
        m_pointLightSRV[i] = sd->allocTable("DeferredLight_PointSRV");
        m_spotLightSRV[i] = sd->allocTable("DeferredLight_SpotSRV");
        if (!m_dirLightSRV[i].isValid() || !m_pointLightSRV[i].isValid() || !m_spotLightSRV[i].isValid()){
            LOG("DeferredLightingPass: light SRV alloc failed");
            return false;
        }
        makeStructuredSRV(m_dirLightSRV[i], 0, m_dirLightBuf[i].Get(),
                           MeshPipeline::MAX_DIR_LIGHTS, sizeof(MeshPipeline::GPUDirectionalLight));
        makeStructuredSRV(m_pointLightSRV[i], 0, m_pointLightBuf[i].Get(),
                           MeshPipeline::MAX_POINT_LIGHTS, sizeof(MeshPipeline::GPUPointLight));
        makeStructuredSRV(m_spotLightSRV[i], 0, m_spotLightBuf[i].Get(),
                           MeshPipeline::MAX_SPOT_LIGHTS, sizeof(MeshPipeline::GPUSpotLight));
    }
    return true;
}

bool DeferredLightingPass::createFallbackIBL(ID3D12Device* device){
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
        if (FAILED(hr)){
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
        if (FAILED(hr)){
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
                                           || !m_fallbackBRDFSRV.isValid()){
        LOG("DeferredLightingPass: fallback IBL SRV alloc failed");
        return false;
    }
    writeFallbackCubeSRV(m_fallbackIrradianceSRV, 0, m_fallbackCube.Get());
    writeFallbackCubeSRV(m_fallbackPrefilterSRV, 0, m_fallbackCube.Get());
    writeFallbackTex2DSRV(m_fallbackBRDFSRV, 0, m_fallbackTex2D.Get());
    return true;
}

void DeferredLightingPass::uploadLights(const FrameLightData& lights, int viewportIndex){
    auto copy = [](void* dst, const void* src, size_t count, size_t stride, size_t maxCount){
        UINT n = static_cast<UINT>(std::min(count, maxCount));
        if (n > 0) memcpy(dst, src, n * stride);
    };
    copy(m_dirLightMapped[viewportIndex], lights.dirLights.data(), lights.dirLights.size(),
         sizeof(MeshPipeline::GPUDirectionalLight), MeshPipeline::MAX_DIR_LIGHTS);
    copy(m_pointLightMapped[viewportIndex], lights.pointLights.data(), lights.pointLights.size(),
         sizeof(MeshPipeline::GPUPointLight), MeshPipeline::MAX_POINT_LIGHTS);
    copy(m_spotLightMapped[viewportIndex], lights.spotLights.data(), lights.spotLights.size(),
         sizeof(MeshPipeline::GPUSpotLight), MeshPipeline::MAX_SPOT_LIGHTS);
}

void DeferredLightingPass::uploadPerFrameCB(const FrameLightData& lights,
                                             const Vector3& cameraPos,
                                             const Matrix& invViewProj,
                                             uint32_t envRoughLevels,
                                             uint32_t width, uint32_t height,
                                             int viewportIndex){
    CbPerFrame cb = {};
    cb.dirLightCount = static_cast<uint32_t>(std::min(lights.dirLights.size(), (size_t)MeshPipeline::MAX_DIR_LIGHTS));
    cb.pointLightCount = static_cast<uint32_t>(std::min(lights.pointLights.size(), (size_t)MeshPipeline::MAX_POINT_LIGHTS));
    cb.spotLightCount = static_cast<uint32_t>(std::min(lights.spotLights.size(), (size_t)MeshPipeline::MAX_SPOT_LIGHTS));
    cb.envRoughnessLevels = envRoughLevels;
    cb.cameraPosition = cameraPos;
    cb.framePad = 0;
    cb.invViewProj = invViewProj.Transpose();
    cb.viewportWidth = width;
    cb.viewportHeight = height;
    cb.pad0 = cb.pad1 = 0;
    memcpy(m_perFrameMapped[viewportIndex], &cb, sizeof(cb));
}

void DeferredLightingPass::render(ID3D12GraphicsCommandList* cmd,
                                   GBufferPass& gbufferPass,
                                   const FrameLightData& lights,
                                   const Vector3& cameraPos,
                                   const Matrix& view,
                                   const Matrix& projection,
                                   const Matrix& invViewProj,
                                   const EnvironmentSystem* env,
                                   uint32_t width, uint32_t height,
                                   int viewportIndex){
    if (width == 0 || height == 0) return;
    if (!gbufferPass.getGBuffer().isValid()) return;

    viewportIndex = (viewportIndex >= 0 && viewportIndex < NUM_VIEWPORTS) ? viewportIndex : 0;

    uint32_t roughLevels = 0;
    if (env && env->hasIBL()) roughLevels = EnvironmentMap::NUM_ROUGHNESS_LEVELS;

    m_lightCulling.cull(cmd, gbufferPass, lights, view, projection, width, height, viewportIndex);

    uploadLights(lights, viewportIndex);
    uploadPerFrameCB(lights, cameraPos, invViewProj, roughLevels, width, height, viewportIndex);

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
                                            m_perFrameCB[viewportIndex]->GetGPUVirtualAddress());

    cmd->SetGraphicsRootDescriptorTable(DeferredLightingPipeline::SLOT_DIR_LIGHTS,
                                         m_dirLightSRV[viewportIndex].getGPUHandle(0));
    cmd->SetGraphicsRootDescriptorTable(DeferredLightingPipeline::SLOT_POINT_LIGHTS,
                                         m_pointLightSRV[viewportIndex].getGPUHandle(0));
    cmd->SetGraphicsRootDescriptorTable(DeferredLightingPipeline::SLOT_SPOT_LIGHTS,
                                         m_spotLightSRV[viewportIndex].getGPUHandle(0));

    if (env && env->hasIBL()){
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

    cmd->SetGraphicsRootDescriptorTable(DeferredLightingPipeline::SLOT_POINT_INDICES,
                                         m_lightCulling.getPointListSRV(viewportIndex));
    cmd->SetGraphicsRootDescriptorTable(DeferredLightingPipeline::SLOT_SPOT_INDICES,
                                         m_lightCulling.getSpotListSRV(viewportIndex));

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

#include "Globals.h"
#include "ModuleSamplerHeap.h"
#include "ReadData.h"
#include <d3dx12.h>

bool DeferredLightingPipeline::init(ID3D12Device* device){
    return createRootSignature(device) && createPSO(device);
}

bool DeferredLightingPipeline::createRootSignature(ID3D12Device* device){
    CD3DX12_DESCRIPTOR_RANGE dirRange; dirRange.Init (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE pointRange; pointRange.Init (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    CD3DX12_DESCRIPTOR_RANGE spotRange; spotRange.Init (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
    CD3DX12_DESCRIPTOR_RANGE irrRange; irrRange.Init (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
    CD3DX12_DESCRIPTOR_RANGE prefRange; prefRange.Init (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);
    CD3DX12_DESCRIPTOR_RANGE brdfRange; brdfRange.Init (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5);
    CD3DX12_DESCRIPTOR_RANGE albRange; albRange.Init (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6);
    CD3DX12_DESCRIPTOR_RANGE normRange; normRange.Init (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 7);
    CD3DX12_DESCRIPTOR_RANGE emissRange; emissRange.Init (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 8);
    CD3DX12_DESCRIPTOR_RANGE depthRange; depthRange .Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 9);
    CD3DX12_DESCRIPTOR_RANGE pointIdxRange; pointIdxRange .Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 10);
    CD3DX12_DESCRIPTOR_RANGE spotIdxRange; spotIdxRange .Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 11);
    CD3DX12_DESCRIPTOR_RANGE samplerRange; samplerRange .Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
                                                                  ModuleSamplerHeap::COUNT, 0);

    CD3DX12_ROOT_PARAMETER params[14];
    params[SLOT_PERFRAME_CB ].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_DIR_LIGHTS ].InitAsDescriptorTable(1, &dirRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_POINT_LIGHTS ].InitAsDescriptorTable(1, &pointRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_SPOT_LIGHTS ].InitAsDescriptorTable(1, &spotRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_IRRADIANCE ].InitAsDescriptorTable(1, &irrRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_PREFILTER ].InitAsDescriptorTable(1, &prefRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_BRDF_LUT ].InitAsDescriptorTable(1, &brdfRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_GBUF_ALBEDO ].InitAsDescriptorTable(1, &albRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_GBUF_NORMAL ].InitAsDescriptorTable(1, &normRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_GBUF_EMISSIVE ].InitAsDescriptorTable(1, &emissRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_GBUF_DEPTH ].InitAsDescriptorTable(1, &depthRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_POINT_INDICES ].InitAsDescriptorTable(1, &pointIdxRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_SPOT_INDICES ].InitAsDescriptorTable(1, &spotIdxRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_SAMPLER ].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(params), params, 0, nullptr,
              D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    if (FAILED(hr)){
        if (error) OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
        LOG("DeferredLightingPipeline: serialize root sig failed 0x%08X", hr);
        return false;
    }
    hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                      IID_PPV_ARGS(&m_rootSig));
    if (FAILED(hr)){
        LOG("DeferredLightingPipeline: CreateRootSignature failed 0x%08X", hr);
        return false;
    }
    return true;
}

bool DeferredLightingPipeline::createPSO(ID3D12Device* device){
    auto vs = DX::ReadData(L"DeferredLightingVS.cso");
    auto ps = DX::ReadData(L"DeferredLightingPS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = m_rootSig.Get();
    desc.InputLayout = { nullptr, 0 };
    desc.VS = { vs.data(), vs.size() };
    desc.PS = { ps.data(), ps.size() };
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc = { 1, 0 };
    desc.SampleMask = UINT_MAX;

    desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    HRESULT hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_pso));
    if (FAILED(hr)){
        LOG("DeferredLightingPipeline: CreateGraphicsPipelineState failed 0x%08X", hr);
        return false;
    }
    return true;
}
