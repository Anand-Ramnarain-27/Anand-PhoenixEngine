#include "Globals.h"
#include "IBLGenerator.h"
#include "EnvironmentMap.h"
#include "IBLSettings.h"
#include "FaceProjection.h"
#include "D3D12ResourceFactory.h"
#include "CubemapPipelineBuilder.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleGPUResources.h"
#include "ModuleRTDescriptors.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplerHeap.h"
#include "CubeGeometry.h"
#include "ReadData.h"
#include <algorithm>

using namespace DirectX;

bool IBLGenerator::ensureGeometry(ID3D12Device* device) {
    if (m_geometryReady)
        return true;

    auto* resources = app->getGPUResources();
    m_cubeVB = resources->createDefaultBuffer(CubeGeometry::kCubeVerts, CubeGeometry::kCubeVertexSize, "IBL_CubeVB");

    if (!m_cubeVB) {
        LOG("IBLGenerator: failed to create cube VB");
        return false;
    }

    m_vbView.BufferLocation = m_cubeVB->GetGPUVirtualAddress();
    m_vbView.StrideInBytes = CubeGeometry::kCubeVertexStride;
    m_vbView.SizeInBytes = CubeGeometry::kCubeVertexSize;
    m_geometryReady = true;
    return true;
}

bool IBLGenerator::ensureFaceCB(ID3D12Device* device) {
    if (m_faceCB)
        return true;

    FaceCB zero{};
    auto* resources = app->getGPUResources();
    m_faceCB = resources->createUploadBuffer(&zero, sizeof(FaceCB), "IBL_FaceCB");

    if (!m_faceCB) {
        LOG("IBLGenerator: failed to create face CB");
        return false;
    }

    m_faceCB->Map(0, nullptr, reinterpret_cast<void**>(&m_faceCBPtr));
    return true;
}

void IBLGenerator::renderCubeFace(
    ID3D12Device* device, ID3D12GraphicsCommandList* cmd,
    ID3D12Resource* target, uint32_t faceIndex, uint32_t mipLevel,
    uint32_t totalMips, uint32_t baseFaceSize, float roughness,
    ID3D12RootSignature* rs, ID3D12PipelineState* pso,
    D3D12_GPU_DESCRIPTOR_HANDLE sourceSRV, DXGI_FORMAT rtvFmt)
{
    auto* rtDescs = app->getRTDescriptors();
    auto* samplers = app->getSamplerHeap();
    uint32_t mipSize = std::max(1u, baseFaceSize >> mipLevel);

    RenderTargetDesc rtv = rtDescs->create(target, faceIndex, mipLevel, rtvFmt);
    if (!rtv) {
        LOG("IBLGenerator: RTV alloc failed (face %u, mip %u)", faceIndex, mipLevel);
        return;
    }

    UINT subRes = D3D12CalcSubresource(mipLevel, faceIndex, 0, totalMips, 6);
    auto barrierIn = CD3DX12_RESOURCE_BARRIER::Transition(
        target, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET, subRes);
    cmd->ResourceBarrier(1, &barrierIn);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtv.getCPUHandle();
    cmd->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    float clearColor[4] = { 0, 0, 0, 1 };
    cmd->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    D3D12_VIEWPORT vp = { 0, 0, float(mipSize), float(mipSize), 0, 1 };
    D3D12_RECT     sc = { 0, 0, LONG(mipSize),  LONG(mipSize) };
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);

    XMMATRIX vp_m = FaceProjection::viewProj(faceIndex);
    m_faceCBPtr->roughness = roughness;
    m_faceCBPtr->flipX = FaceProjection::needsFlipX(faceIndex) ? 1 : 0;
    m_faceCBPtr->flipZ = FaceProjection::needsFlipZ(faceIndex) ? 1 : 0;
    XMStoreFloat4x4(&m_faceCBPtr->vp, XMMatrixTranspose(vp_m));

    ID3D12DescriptorHeap* heaps[] = { app->getShaderDescriptors()->getHeap(), samplers->getHeap() };
    cmd->SetDescriptorHeaps(2, heaps);
    cmd->SetGraphicsRootSignature(rs);
    cmd->SetPipelineState(pso);
    cmd->SetGraphicsRootConstantBufferView(0, m_faceCB->GetGPUVirtualAddress());
    cmd->SetGraphicsRootDescriptorTable(1, sourceSRV);
    cmd->SetGraphicsRootDescriptorTable(2, samplers->getGPUHandle(ModuleSamplerHeap::LINEAR_WRAP));
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 1, &m_vbView);
    cmd->DrawInstanced(36, 1, 0, 0);

    auto barrierOut = CD3DX12_RESOURCE_BARRIER::Transition(
        target, D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, subRes);
    cmd->ResourceBarrier(1, &barrierOut);
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static bool setupIBLResources(
    ID3D12Device* device, EnvironmentMap& env,
    ComPtr<ID3D12RootSignature>& irradianceRS, ComPtr<ID3D12PipelineState>& irradiancePSO,
    ComPtr<ID3D12RootSignature>& prefilterRS, ComPtr<ID3D12PipelineState>& prefilterPSO,
    ComPtr<ID3D12RootSignature>& brdfRS, ComPtr<ID3D12PipelineState>& brdfPSO)
{
    if (!D3D12ResourceFactory::createCubemapRT(device, IBLSettings::IrradianceSize, 1,
        DXGI_FORMAT_R16G16B16A16_FLOAT, L"IrradianceCubemap", env.irradianceCubemap)) {
        LOG("IBLGenerator: failed to create irradiance cubemap");
        return false;
    }

    if (!D3D12ResourceFactory::createCubemapRT(device, IBLSettings::PrefilterSize, IBLSettings::NumRoughnessLevels,
        DXGI_FORMAT_R16G16B16A16_FLOAT, L"PrefilteredEnvCubemap", env.prefilteredCubemap)) {
        LOG("IBLGenerator: failed to create prefiltered cubemap");
        return false;
    }

    if (!D3D12ResourceFactory::create2DRT(device, IBLSettings::BRDFLUTSize,
        DXGI_FORMAT_R16G16_FLOAT, L"BRDFIntegrationLUT", env.brdfLUT)) {
        LOG("IBLGenerator: failed to create BRDF LUT");
        return false;
    }

    irradiancePSO.Reset(); irradianceRS.Reset();
    prefilterPSO.Reset();  prefilterRS.Reset();
    brdfPSO.Reset();       brdfRS.Reset();

    if (!CubemapPipelineBuilder::buildCubeFacePipeline(device, L"IrradianceMapPS.cso",
        DXGI_FORMAT_R16G16B16A16_FLOAT, irradianceRS, irradiancePSO)) {
        LOG("IBLGenerator: failed to create irradiance pipeline");
        return false;
    }

    if (!CubemapPipelineBuilder::buildCubeFacePipeline(device, L"PrefilterEnvMapPS.cso",
        DXGI_FORMAT_R16G16B16A16_FLOAT, prefilterRS, prefilterPSO)) {
        LOG("IBLGenerator: failed to create prefilter pipeline");
        return false;
    }

    if (!CubemapPipelineBuilder::buildBRDFPipeline(device, brdfRS, brdfPSO)) {
        LOG("IBLGenerator: failed to create BRDF pipeline");
        return false;
    }

    return true;
}

static bool writeSRVs(EnvironmentMap& env) {
    auto* shaderDescs = app->getShaderDescriptors();

    env.irradianceSRVTable = shaderDescs->allocTable("IBL_Irradiance");
    if (!env.irradianceSRVTable.isValid()) { LOG("IBLGenerator: failed to alloc irradiance SRV table"); return false; }
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.TextureCube.MipLevels = 1;
        srvDesc.TextureCube.MostDetailedMip = 0;
        env.irradianceSRVTable.createSRV(env.irradianceCubemap.Get(), 0, &srvDesc);
    }

    env.prefilteredSRVTable = shaderDescs->allocTable("IBL_Prefilter");
    if (!env.prefilteredSRVTable.isValid()) { LOG("IBLGenerator: failed to alloc prefilter SRV table"); return false; }
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.TextureCube.MipLevels = IBLSettings::NumRoughnessLevels;
        srvDesc.TextureCube.MostDetailedMip = 0;
        env.prefilteredSRVTable.createSRV(env.prefilteredCubemap.Get(), 0, &srvDesc);
    }

    env.brdfLUTSRVTable = shaderDescs->allocTable("IBL_BRDF_LUT");
    if (!env.brdfLUTSRVTable.isValid()) { LOG("IBLGenerator: failed to alloc BRDF LUT SRV table"); return false; }
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        env.brdfLUTSRVTable.createSRV(env.brdfLUT.Get(), 0, &srvDesc);
    }

    return true;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool IBLGenerator::generate(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, EnvironmentMap& env) {
    LOG("IBLGenerator: starting IBL bake...");

    if (!env.isValid()) { LOG("IBLGenerator: source environment map is not valid"); return false; }
    if (!ensureGeometry(device)) return false;
    if (!ensureFaceCB(device))   return false;

    if (!setupIBLResources(device, env,
        m_irradianceRS, m_irradiancePSO,
        m_prefilterRS, m_prefilterPSO,
        m_brdfRS, m_brdfPSO))
        return false;

    LOG("IBLGenerator: baking irradiance map...");
    for (uint32_t face = 0; face < 6; ++face)
        renderCubeFace(device, cmd, env.irradianceCubemap.Get(), face, 0, 1,
            IBLSettings::IrradianceSize, 0.0f,
            m_irradianceRS.Get(), m_irradiancePSO.Get(),
            env.srvTable.getGPUHandle(), DXGI_FORMAT_R16G16B16A16_FLOAT);

    LOG("IBLGenerator: baking pre-filtered env map (%u roughness levels)...", IBLSettings::NumRoughnessLevels);
    for (uint32_t mip = 0; mip < IBLSettings::NumRoughnessLevels; ++mip) {
        float roughness = (IBLSettings::NumRoughnessLevels > 1)
            ? float(mip) / float(IBLSettings::NumRoughnessLevels - 1) : 0.0f;

        for (uint32_t face = 0; face < 6; ++face)
            renderCubeFace(device, cmd, env.prefilteredCubemap.Get(), face, mip,
                IBLSettings::NumRoughnessLevels, IBLSettings::PrefilterSize, roughness,
                m_prefilterRS.Get(), m_prefilterPSO.Get(),
                env.srvTable.getGPUHandle(), DXGI_FORMAT_R16G16B16A16_FLOAT);
    }

    LOG("IBLGenerator: baking BRDF integration LUT...");
    if (!bakeBRDFLut(device, cmd, env)) return false;
    if (!writeSRVs(env))                return false;

    LOG("IBLGenerator: IBL bake complete.");
    return true;
}

bool IBLGenerator::prepareResources(ID3D12Device* device, EnvironmentMap& env) {
    if (!env.isValid()) { LOG("IBLGenerator: source environment map is not valid"); return false; }
    if (!ensureGeometry(device)) return false;
    if (!ensureFaceCB(device))   return false;

    return setupIBLResources(device, env,
        m_irradianceRS, m_irradiancePSO,
        m_prefilterRS, m_prefilterPSO,
        m_brdfRS, m_brdfPSO);
}

bool IBLGenerator::bakeIrradiance(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, EnvironmentMap& env) {
    auto* samplers = app->getSamplerHeap();

    ID3D12DescriptorHeap* heaps[] = { app->getShaderDescriptors()->getHeap(), samplers->getHeap() };
    cmd->SetDescriptorHeaps(2, heaps);

    LOG("IBLGenerator: baking irradiance map...");
    for (uint32_t face = 0; face < 6; ++face)
        renderCubeFace(device, cmd, env.irradianceCubemap.Get(), face, 0, 1,
            IBLSettings::IrradianceSize, 0.0f,
            m_irradianceRS.Get(), m_irradiancePSO.Get(),
            env.srvTable.getGPUHandle(), DXGI_FORMAT_R16G16B16A16_FLOAT);

    return true;
}

bool IBLGenerator::bakePrefilter(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, EnvironmentMap& env, uint32_t mipIndex) {
    auto* samplers = app->getSamplerHeap();

    ID3D12DescriptorHeap* heaps[] = { app->getShaderDescriptors()->getHeap(), samplers->getHeap() };
    cmd->SetDescriptorHeaps(2, heaps);

    float roughness = (IBLSettings::NumRoughnessLevels > 1)
        ? float(mipIndex) / float(IBLSettings::NumRoughnessLevels - 1) : 0.0f;

    LOG("IBLGenerator: baking prefilter mip %u (roughness=%.2f)...", mipIndex, roughness);
    for (uint32_t face = 0; face < 6; ++face)
        renderCubeFace(device, cmd, env.prefilteredCubemap.Get(), face, mipIndex,
            IBLSettings::NumRoughnessLevels, IBLSettings::PrefilterSize, roughness,
            m_prefilterRS.Get(), m_prefilterPSO.Get(),
            env.srvTable.getGPUHandle(), DXGI_FORMAT_R16G16B16A16_FLOAT);

    return true;
}

bool IBLGenerator::bakeBRDFLut(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, EnvironmentMap& env) {
    auto* rtDescs = app->getRTDescriptors();
    auto* shaderDescs = app->getShaderDescriptors();
    auto* samplers = app->getSamplerHeap();

    // BRDF LUT pipeline has no sampler table, but we still bind both heaps
    // so the command list state stays consistent
    ID3D12DescriptorHeap* heaps[] = { shaderDescs->getHeap(), samplers->getHeap() };
    cmd->SetDescriptorHeaps(2, heaps);

    LOG("IBLGenerator: baking BRDF integration LUT...");

    RenderTargetDesc rtv = rtDescs->create(env.brdfLUT.Get());
    if (!rtv) { LOG("IBLGenerator: BRDF LUT RTV alloc failed"); return false; }

    auto barrierIn = CD3DX12_RESOURCE_BARRIER::Transition(
        env.brdfLUT.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmd->ResourceBarrier(1, &barrierIn);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtv.getCPUHandle();
    cmd->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    float clear[4] = { 0, 0, 0, 1 };
    cmd->ClearRenderTargetView(rtvHandle, clear, 0, nullptr);

    D3D12_VIEWPORT vp = { 0, 0, float(IBLSettings::BRDFLUTSize), float(IBLSettings::BRDFLUTSize), 0, 1 };
    D3D12_RECT     sc = { 0, 0, LONG(IBLSettings::BRDFLUTSize),  LONG(IBLSettings::BRDFLUTSize) };
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);

    cmd->SetGraphicsRootSignature(m_brdfRS.Get());
    cmd->SetPipelineState(m_brdfPSO.Get());
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 0, nullptr);
    cmd->DrawInstanced(3, 1, 0, 0);

    auto barrierOut = CD3DX12_RESOURCE_BARRIER::Transition(
        env.brdfLUT.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmd->ResourceBarrier(1, &barrierOut);

    return true;
}

bool IBLGenerator::finaliseSRVs(EnvironmentMap& env) {
    if (!writeSRVs(env)) return false;
    LOG("IBLGenerator: IBL bake complete.");
    return true;
}