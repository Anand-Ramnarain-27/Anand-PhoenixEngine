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
#include <algorithm>

using namespace DirectX;

// ---------------------------------------------------------------------------
// Resource creation helpers
// ---------------------------------------------------------------------------

bool IBLGenerator::ensureGeometry(ID3D12Device* device)
{
    if (m_geometryReady)
        return true;

    auto* resources = app->getGPUResources();
    m_cubeVB = resources->createDefaultBuffer(
        CubeGeometry::kCubeVerts, CubeGeometry::kCubeVertexSize, "IBL_CubeVB");

    if (!m_cubeVB)
    {
        LOG("IBLGenerator: failed to create cube vertex buffer");
        return false;
    }

    m_vbView.BufferLocation = m_cubeVB->GetGPUVirtualAddress();
    m_vbView.StrideInBytes = CubeGeometry::kCubeVertexStride;
    m_vbView.SizeInBytes = CubeGeometry::kCubeVertexSize;
    m_geometryReady = true;
    return true;
}

bool IBLGenerator::ensureFaceCB(ID3D12Device* device)
{
    if (m_faceCB)
        return true;

    FaceCB zero{};
    auto* resources = app->getGPUResources();
    m_faceCB = resources->createUploadBuffer(&zero, sizeof(FaceCB), "IBL_FaceCB");

    if (!m_faceCB)
    {
        LOG("IBLGenerator: failed to create face constant buffer");
        return false;
    }

    m_faceCB->Map(0, nullptr, reinterpret_cast<void**>(&m_faceCBPtr));
    return true;
}

bool IBLGenerator::ensurePassCB(ID3D12Device* device)
{
    if (m_passCB)
        return true;

    PassCB zero{};
    auto* resources = app->getGPUResources();
    m_passCB = resources->createUploadBuffer(&zero, sizeof(PassCB), "IBL_PassCB");

    if (!m_passCB)
    {
        LOG("IBLGenerator: failed to create pass constant buffer");
        return false;
    }

    m_passCB->Map(0, nullptr, reinterpret_cast<void**>(&m_passCBPtr));
    return true;
}

// ---------------------------------------------------------------------------
// GPU resource + pipeline allocation for all IBL targets
// ---------------------------------------------------------------------------

static bool allocateIBLResources(
    ID3D12Device* device,
    EnvironmentMap& env,
    ComPtr<ID3D12RootSignature>& irradianceRS, ComPtr<ID3D12PipelineState>& irradiancePSO,
    ComPtr<ID3D12RootSignature>& prefilterRS, ComPtr<ID3D12PipelineState>& prefilterPSO,
    ComPtr<ID3D12RootSignature>& brdfRS, ComPtr<ID3D12PipelineState>& brdfPSO)
{
    // Irradiance cubemap: single mip, small resolution — diffuse convolution
    if (!D3D12ResourceFactory::createCubemapRT(device, IBLSettings::IrradianceSize, 1,
        DXGI_FORMAT_R16G16B16A16_FLOAT, L"IrradianceCubemap", env.irradianceCubemap))
    {
        LOG("IBLGenerator: failed to create irradiance cubemap resource");
        return false;
    }

    // Pre-filtered env cubemap: one mip per roughness level — specular convolution
    if (!D3D12ResourceFactory::createCubemapRT(device, IBLSettings::PrefilterSize, IBLSettings::NumRoughnessLevels,
        DXGI_FORMAT_R16G16B16A16_FLOAT, L"PrefilteredEnvCubemap", env.prefilteredCubemap))
    {
        LOG("IBLGenerator: failed to create pre-filtered env cubemap resource");
        return false;
    }

    // BRDF integration LUT: 2D texture storing GGX split-sum scale (R) and bias (G)
    if (!D3D12ResourceFactory::create2DRT(device, IBLSettings::BRDFLUTSize,
        DXGI_FORMAT_R16G16_FLOAT, L"BRDFIntegrationLUT", env.brdfLUT))
    {
        LOG("IBLGenerator: failed to create BRDF integration LUT resource");
        return false;
    }

    // Always rebuild pipelines when (re)allocating resources
    irradiancePSO.Reset();  irradianceRS.Reset();
    prefilterPSO.Reset();   prefilterRS.Reset();
    brdfPSO.Reset();        brdfRS.Reset();

    // IrradianceMapPS.cso — cosine-weighted hemisphere sampling
    if (!CubemapPipelineBuilder::buildCubeFacePipeline(device, L"IrradianceMapPS.cso",
        DXGI_FORMAT_R16G16B16A16_FLOAT, irradianceRS, irradiancePSO))
    {
        LOG("IBLGenerator: failed to build irradiance pipeline");
        return false;
    }

    // PrefilterEnvMapPS.cso — GGX importance sampling per roughness level
    if (!CubemapPipelineBuilder::buildCubeFacePipeline(device, L"PrefilterEnvMapPS.cso",
        DXGI_FORMAT_R16G16B16A16_FLOAT, prefilterRS, prefilterPSO))
    {
        LOG("IBLGenerator: failed to build pre-filter pipeline");
        return false;
    }

    // EnvironmentBRDFPS.cso — split-sum BRDF integration (fullscreen triangle, no inputs)
    if (!CubemapPipelineBuilder::buildBRDFPipeline(device, brdfRS, brdfPSO))
    {
        LOG("IBLGenerator: failed to build BRDF LUT pipeline");
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// SRV finalisation — writes TextureCube / Texture2D views into the env tables
// ---------------------------------------------------------------------------

static bool writeSRVs(EnvironmentMap& env)
{
    auto* shaderDescs = app->getShaderDescriptors();

    // Irradiance cubemap SRV
    env.irradianceSRVTable = shaderDescs->allocTable("IBL_Irradiance");
    if (!env.irradianceSRVTable.isValid())
    {
        LOG("IBLGenerator: failed to allocate irradiance SRV table");
        return false;
    }
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.TextureCube.MipLevels = 1;
        srvDesc.TextureCube.MostDetailedMip = 0;
        env.irradianceSRVTable.createSRV(env.irradianceCubemap.Get(), 0, &srvDesc);
    }

    // Pre-filtered env cubemap SRV (all roughness mip levels)
    env.prefilteredSRVTable = shaderDescs->allocTable("IBL_Prefilter");
    if (!env.prefilteredSRVTable.isValid())
    {
        LOG("IBLGenerator: failed to allocate pre-filter SRV table");
        return false;
    }
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.TextureCube.MipLevels = IBLSettings::NumRoughnessLevels;
        srvDesc.TextureCube.MostDetailedMip = 0;
        env.prefilteredSRVTable.createSRV(env.prefilteredCubemap.Get(), 0, &srvDesc);
    }

    // BRDF LUT SRV
    env.brdfLUTSRVTable = shaderDescs->allocTable("IBL_BRDF_LUT");
    if (!env.brdfLUTSRVTable.isValid())
    {
        LOG("IBLGenerator: failed to allocate BRDF LUT SRV table");
        return false;
    }
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
// Core face render
//
// Root signature slot mapping (CubemapPipelineBuilder::buildCubeFacePipeline):
//   0: CBV b0  - FaceCB  (vp, flipX, flipZ, roughness)  read by SkyboxVS
//   1: CBV b2  - PassCB  (roughness, numSamples, cubemapSize, lodBias)  read by PS
//   2: SRV t0  - source cubemap
//   3: Sampler s0-s3
// ---------------------------------------------------------------------------

void IBLGenerator::renderCubeFace(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmd,
    ID3D12Resource* target,
    uint32_t                    faceIndex,
    uint32_t                    mipLevel,
    uint32_t                    totalMips,
    uint32_t                    baseFaceSize,
    float                       roughness,
    ID3D12RootSignature* rs,
    ID3D12PipelineState* pso,
    D3D12_GPU_DESCRIPTOR_HANDLE sourceSRV,
    DXGI_FORMAT                 rtvFmt,
    int                         numSamples,
    int                         cubemapSize)
{
    auto* rtDescs = app->getRTDescriptors();
    auto* samplers = app->getSamplerHeap();
    uint32_t mipSize = std::max(1u, baseFaceSize >> mipLevel);

    RenderTargetDesc rtv = rtDescs->create(target, faceIndex, mipLevel, rtvFmt);
    if (!rtv)
    {
        LOG("IBLGenerator: RTV alloc failed (face=%u mip=%u)", faceIndex, mipLevel);
        return;
    }

    UINT subRes = D3D12CalcSubresource(mipLevel, faceIndex, 0, totalMips, 6);

    auto barrierToRT = CD3DX12_RESOURCE_BARRIER::Transition(
        target, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET, subRes);
    cmd->ResourceBarrier(1, &barrierToRT);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtv.getCPUHandle();
    cmd->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    cmd->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    D3D12_VIEWPORT vp = { 0.0f, 0.0f, float(mipSize), float(mipSize), 0.0f, 1.0f };
    D3D12_RECT     sc = { 0,    0,    LONG(mipSize),  LONG(mipSize) };
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);

    // Update face CB — FaceCB layout must match SkyboxCB in SkyboxVS.hlsl (b0: vp, flipX, flipZ)
    XMMATRIX vpMatrix = FaceProjection::viewProj(faceIndex);
    XMStoreFloat4x4(&m_faceCBPtr->vp, XMMatrixTranspose(vpMatrix));
    m_faceCBPtr->flipX = FaceProjection::needsFlipX(faceIndex) ? 1 : 0;
    m_faceCBPtr->flipZ = FaceProjection::needsFlipZ(faceIndex) ? 1 : 0;
    m_faceCBPtr->roughness = roughness;

    // Update pass CB — PassCB layout matches cbuffer Constants (b2) in IBL pixel shaders:
    //   PrefilterEnvMapPS: { float roughness; int numSamples; int cubemapSize; float lodBias; }
    //   IrradianceMapPS:   { int numSamples; int cubemapSize; float lodBias; int padding; }
    // Using the prefilter layout as superset; roughness is simply ignored by the irradiance shader.
    m_passCBPtr->roughness = roughness;
    m_passCBPtr->numSamples = numSamples;
    m_passCBPtr->cubemapSize = cubemapSize;
    m_passCBPtr->lodBias = 0.0f;

    ID3D12DescriptorHeap* heaps[] = { app->getShaderDescriptors()->getHeap(), samplers->getHeap() };
    cmd->SetDescriptorHeaps(2, heaps);
    cmd->SetGraphicsRootSignature(rs);
    cmd->SetPipelineState(pso);
    cmd->SetGraphicsRootConstantBufferView(0, m_faceCB->GetGPUVirtualAddress());  // b0: FaceCB
    cmd->SetGraphicsRootConstantBufferView(1, m_passCB->GetGPUVirtualAddress());  // b2: PassCB
    cmd->SetGraphicsRootDescriptorTable(2, sourceSRV);                             // t0: source cubemap
    cmd->SetGraphicsRootDescriptorTable(3, samplers->getGPUHandle(ModuleSamplerHeap::LINEAR_WRAP));

    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 1, &m_vbView);
    cmd->DrawInstanced(CubeGeometry::kCubeVertexCount, 1, 0, 0);

    auto barrierToSRV = CD3DX12_RESOURCE_BARRIER::Transition(
        target, D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, subRes);
    cmd->ResourceBarrier(1, &barrierToSRV);
}

// ---------------------------------------------------------------------------
// Public API — single-shot bake (source cubemap already exists)
// ---------------------------------------------------------------------------

bool IBLGenerator::generate(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, EnvironmentMap& env)
{
    LOG("IBLGenerator: starting IBL bake...");

    if (!env.isValid()) { LOG("IBLGenerator: source environment map is not valid"); return false; }
    if (!ensureGeometry(device))  return false;
    if (!ensureFaceCB(device))    return false;
    if (!ensurePassCB(device))    return false;

    if (!allocateIBLResources(device, env,
        m_irradianceRS, m_irradiancePSO,
        m_prefilterRS, m_prefilterPSO,
        m_brdfRS, m_brdfPSO))
        return false;

    LOG("IBLGenerator: baking irradiance map...");
    for (uint32_t face = 0; face < 6; ++face)
        renderCubeFace(device, cmd,
            env.irradianceCubemap.Get(), face, 0, 1,
            IBLSettings::IrradianceSize, 0.0f,
            m_irradianceRS.Get(), m_irradiancePSO.Get(),
            env.srvTable.getGPUHandle(), DXGI_FORMAT_R16G16B16A16_FLOAT,
            1024, int(IBLSettings::IrradianceSize));

    LOG("IBLGenerator: baking pre-filtered env map (%u roughness levels)...", IBLSettings::NumRoughnessLevels);
    for (uint32_t mip = 0; mip < IBLSettings::NumRoughnessLevels; ++mip)
    {
        float roughness = (IBLSettings::NumRoughnessLevels > 1)
            ? float(mip) / float(IBLSettings::NumRoughnessLevels - 1)
            : 0.0f;

        for (uint32_t face = 0; face < 6; ++face)
            renderCubeFace(device, cmd,
                env.prefilteredCubemap.Get(), face, mip,
                IBLSettings::NumRoughnessLevels, IBLSettings::PrefilterSize, roughness,
                m_prefilterRS.Get(), m_prefilterPSO.Get(),
                env.srvTable.getGPUHandle(), DXGI_FORMAT_R16G16B16A16_FLOAT,
                512, int(IBLSettings::PrefilterSize));
    }

    LOG("IBLGenerator: baking BRDF integration LUT...");
    if (!bakeBRDFLut(device, cmd, env)) return false;
    if (!writeSRVs(env))                return false;

    LOG("IBLGenerator: IBL bake complete.");
    return true;
}

// ---------------------------------------------------------------------------
// Public API — stepped bake (used alongside HDR-to-cubemap conversion)
// ---------------------------------------------------------------------------

bool IBLGenerator::prepareResources(ID3D12Device* device, EnvironmentMap& env)
{
    if (!env.isValid()) { LOG("IBLGenerator: source environment map is not valid"); return false; }
    if (!ensureGeometry(device))  return false;
    if (!ensureFaceCB(device))    return false;
    if (!ensurePassCB(device))    return false;

    return allocateIBLResources(device, env,
        m_irradianceRS, m_irradiancePSO,
        m_prefilterRS, m_prefilterPSO,
        m_brdfRS, m_brdfPSO);
}

bool IBLGenerator::bakeIrradiance(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, EnvironmentMap& env)
{
    auto* samplers = app->getSamplerHeap();
    ID3D12DescriptorHeap* heaps[] = { app->getShaderDescriptors()->getHeap(), samplers->getHeap() };
    cmd->SetDescriptorHeaps(2, heaps);

    LOG("IBLGenerator: baking irradiance map...");
    for (uint32_t face = 0; face < 6; ++face)
        renderCubeFace(device, cmd,
            env.irradianceCubemap.Get(), face, 0, 1,
            IBLSettings::IrradianceSize, 0.0f,
            m_irradianceRS.Get(), m_irradiancePSO.Get(),
            env.srvTable.getGPUHandle(), DXGI_FORMAT_R16G16B16A16_FLOAT,
            1024, int(IBLSettings::IrradianceSize));

    return true;
}

bool IBLGenerator::bakePrefilter(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, EnvironmentMap& env, uint32_t mipIndex)
{
    auto* samplers = app->getSamplerHeap();
    ID3D12DescriptorHeap* heaps[] = { app->getShaderDescriptors()->getHeap(), samplers->getHeap() };
    cmd->SetDescriptorHeaps(2, heaps);

    float roughness = (IBLSettings::NumRoughnessLevels > 1)
        ? float(mipIndex) / float(IBLSettings::NumRoughnessLevels - 1)
        : 0.0f;

    LOG("IBLGenerator: baking pre-filter mip %u (roughness=%.2f)...", mipIndex, roughness);
    for (uint32_t face = 0; face < 6; ++face)
        renderCubeFace(device, cmd,
            env.prefilteredCubemap.Get(), face, mipIndex,
            IBLSettings::NumRoughnessLevels, IBLSettings::PrefilterSize, roughness,
            m_prefilterRS.Get(), m_prefilterPSO.Get(),
            env.srvTable.getGPUHandle(), DXGI_FORMAT_R16G16B16A16_FLOAT,
            512, int(IBLSettings::PrefilterSize));

    return true;
}

bool IBLGenerator::bakeBRDFLut(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, EnvironmentMap& env)
{
    auto* rtDescs = app->getRTDescriptors();
    auto* shaderDescs = app->getShaderDescriptors();
    auto* samplers = app->getSamplerHeap();

    ID3D12DescriptorHeap* heaps[] = { shaderDescs->getHeap(), samplers->getHeap() };
    cmd->SetDescriptorHeaps(2, heaps);

    RenderTargetDesc rtv = rtDescs->create(env.brdfLUT.Get());
    if (!rtv)
    {
        LOG("IBLGenerator: BRDF LUT RTV alloc failed");
        return false;
    }

    auto barrierToRT = CD3DX12_RESOURCE_BARRIER::Transition(
        env.brdfLUT.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmd->ResourceBarrier(1, &barrierToRT);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtv.getCPUHandle();
    cmd->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    cmd->ClearRenderTargetView(rtvHandle, clear, 0, nullptr);

    D3D12_VIEWPORT vp = { 0.0f, 0.0f, float(IBLSettings::BRDFLUTSize), float(IBLSettings::BRDFLUTSize), 0.0f, 1.0f };
    D3D12_RECT     sc = { 0,    0,    LONG(IBLSettings::BRDFLUTSize),   LONG(IBLSettings::BRDFLUTSize) };
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);

    // EnvironmentBRDFPS is purely computational — no descriptors needed
    cmd->SetGraphicsRootSignature(m_brdfRS.Get());
    cmd->SetPipelineState(m_brdfPSO.Get());
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 0, nullptr);
    cmd->DrawInstanced(3, 1, 0, 0);  // fullscreen triangle via SV_VertexID in FullScreenVS

    auto barrierToSRV = CD3DX12_RESOURCE_BARRIER::Transition(
        env.brdfLUT.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmd->ResourceBarrier(1, &barrierToSRV);

    return true;
}

bool IBLGenerator::finaliseSRVs(EnvironmentMap& env)
{
    if (!writeSRVs(env))
        return false;

    LOG("IBLGenerator: SRVs finalised.");
    return true;
}