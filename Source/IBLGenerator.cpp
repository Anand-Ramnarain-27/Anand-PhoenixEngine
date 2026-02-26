#include "Globals.h"
#include "IBLGenerator.h"
#include "EnvironmentMap.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleResources.h"
#include "ModuleRTDescriptors.h"
#include "ModuleShaderDescriptors.h"
#include "ReadData.h" 

#include <array>
#include <algorithm>

using namespace DirectX;
 
namespace
{
    struct FaceDesc { XMFLOAT3 front; XMFLOAT3 up; };

    static const std::array<FaceDesc, 6> kFaces =
    { {
        { {  1,  0,  0 }, {  0,  1,  0 } },   // 0: +X
        { { -1,  0,  0 }, {  0,  1,  0 } },   // 1: -X
        { {  0,  1,  0 }, {  0,  0, -1 } },   // 2: +Y
        { {  0, -1,  0 }, {  0,  0,  1 } },   // 3: -Y
        { {  0,  0,  1 }, {  0,  1,  0 } },   // 4: +Z
        { {  0,  0, -1 }, {  0,  1,  0 } },   // 5: -Z
    } };
     
    static const float kCubeVerts[] =
    {
        -1,  1, -1,  -1, -1, -1,   1, -1, -1,
         1, -1, -1,   1,  1, -1,  -1,  1, -1,

        -1, -1,  1,  -1, -1, -1,  -1,  1, -1,
        -1,  1, -1,  -1,  1,  1,  -1, -1,  1,

         1, -1, -1,   1, -1,  1,   1,  1,  1,
         1,  1,  1,   1,  1, -1,   1, -1, -1,

        -1, -1,  1,  -1,  1,  1,   1,  1,  1,
         1,  1,  1,   1, -1,  1,  -1, -1,  1,

        -1,  1, -1,   1,  1, -1,   1,  1,  1,
         1,  1,  1,  -1,  1,  1,  -1,  1, -1,

        -1, -1, -1,  -1, -1,  1,   1, -1, -1,
         1, -1, -1,  -1, -1,  1,   1, -1,  1
    };
}
 
bool IBLGenerator::ensureGeometry(ID3D12Device* device)
{
    if (m_geometryReady) return true;

    auto* resources = app->getResources();
    m_cubeVB = resources->createDefaultBuffer(kCubeVerts, sizeof(kCubeVerts), "IBL_CubeVB");
    if (!m_cubeVB) { LOG("IBLGenerator: failed to create cube VB"); return false; }

    m_vbView.BufferLocation = m_cubeVB->GetGPUVirtualAddress();
    m_vbView.StrideInBytes = sizeof(float) * 3;
    m_vbView.SizeInBytes = sizeof(kCubeVerts);

    m_geometryReady = true;
    return true;
}

bool IBLGenerator::ensureFaceCB(ID3D12Device* device)
{
    if (m_faceCB) return true;
     
    FaceCB zero{};
    auto* resources = app->getResources();
    m_faceCB = resources->createUploadBuffer(&zero, sizeof(FaceCB), "IBL_FaceCB");
    if (!m_faceCB) { LOG("IBLGenerator: failed to create face CB"); return false; }

    m_faceCB->Map(0, nullptr, reinterpret_cast<void**>(&m_faceCBPtr));
    return true;
}
 
bool IBLGenerator::createCubemapResource(
    ID3D12Device* device, uint32_t size, uint32_t mips,
    DXGI_FORMAT fmt, const wchar_t* name,
    ComPtr<ID3D12Resource>& out)
{
    auto desc = CD3DX12_RESOURCE_DESC::Tex2D(
        fmt, size, size,
        6,        
        mips,
        1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

    auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    HRESULT hr = device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        nullptr, IID_PPV_ARGS(&out));

    if (FAILED(hr)) { LOG("IBLGenerator: failed to create cubemap resource"); return false; }
    if (name) out->SetName(name);
    return true;
}

bool IBLGenerator::create2DResource(
    ID3D12Device* device, uint32_t size,
    DXGI_FORMAT fmt, const wchar_t* name,
    ComPtr<ID3D12Resource>& out)
{
    auto desc = CD3DX12_RESOURCE_DESC::Tex2D(
        fmt, size, size,
        1, 1, 1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

    auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    HRESULT hr = device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        nullptr, IID_PPV_ARGS(&out));

    if (FAILED(hr)) { LOG("IBLGenerator: failed to create 2D resource"); return false; }
    if (name) out->SetName(name);
    return true;
}
 
bool IBLGenerator::createConvPipeline(
    ID3D12Device* device,
    const wchar_t* psCsoPath, DXGI_FORMAT rtvFmt,
    ComPtr<ID3D12RootSignature>& outRS,
    ComPtr<ID3D12PipelineState>& outPSO)
{
  
    CD3DX12_ROOT_PARAMETER params[2];
    params[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_DESCRIPTOR_RANGE srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    params[1].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_STATIC_SAMPLER_DESC sampler(
        0,
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP);

    CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
    rsDesc.Init(2, params, 1, &sampler,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> blob, err;
    if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err)))
    {
        LOG("IBLGenerator: RS serialize failed: %s",
            err ? (char*)err->GetBufferPointer() : "?");
        return false;
    }
    if (FAILED(device->CreateRootSignature(0, blob->GetBufferPointer(),
        blob->GetBufferSize(), IID_PPV_ARGS(&outRS))))
        return false;

  
    auto vs = DX::ReadData(L"CubemapConvVS.cso");
    auto ps = DX::ReadData(psCsoPath);

    D3D12_INPUT_ELEMENT_DESC layout =
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
      D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = outRS.Get();
    psoDesc.VS = { vs.data(), vs.size() };
    psoDesc.PS = { ps.data(), ps.size() };
    psoDesc.InputLayout = { &layout, 1 };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.RTVFormats[0] = rtvFmt;
    psoDesc.NumRenderTargets = 1;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    return SUCCEEDED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&outPSO)));
}
 
bool IBLGenerator::createBRDFPipeline(
    ID3D12Device* device,
    ComPtr<ID3D12RootSignature>& outRS,
    ComPtr<ID3D12PipelineState>& outPSO)
{ 
    CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
    rsDesc.Init(0, nullptr, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> blob, err;
    if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err)))
        return false;
    if (FAILED(device->CreateRootSignature(0, blob->GetBufferPointer(),
        blob->GetBufferSize(), IID_PPV_ARGS(&outRS))))
        return false;

    auto vs = DX::ReadData(L"FullScreenTriangleVS.cso");
    auto ps = DX::ReadData(L"EnvironmentBRDFPS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = outRS.Get();
    psoDesc.VS = { vs.data(), vs.size() };
    psoDesc.PS = { ps.data(), ps.size() };
    psoDesc.InputLayout = { nullptr, 0 };  
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16_FLOAT;
    psoDesc.NumRenderTargets = 1;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    return SUCCEEDED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&outPSO)));
}
 
void IBLGenerator::renderCubeFace(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmd,
    ID3D12Resource* target,
    uint32_t                   faceIndex,
    uint32_t                   mipLevel,
    uint32_t                   totalMips,
    uint32_t                   baseFaceSize,
    float                      roughness,
    ID3D12RootSignature* rs,
    ID3D12PipelineState* pso,
    D3D12_GPU_DESCRIPTOR_HANDLE sourceSRV,
    DXGI_FORMAT                rtvFmt)
{
    auto* rtDescs = app->getRTDescriptors();

    uint32_t mipSize = std::max(1u, baseFaceSize >> mipLevel);
     
    RenderTargetDesc rtv = rtDescs->create(target, faceIndex, mipLevel, rtvFmt);
    if (!rtv)
    {
        LOG("IBLGenerator: RTV alloc failed (face %u, mip %u)", faceIndex, mipLevel);
        return;
    }
     
    UINT subRes = D3D12CalcSubresource(mipLevel, faceIndex, 0, totalMips, 6);

    auto barrierIn = CD3DX12_RESOURCE_BARRIER::Transition(
        target,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        subRes);
    cmd->ResourceBarrier(1, &barrierIn);
     
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtv.getCPUHandle();
    cmd->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    float clearColor[4] = { 0, 0, 0, 1 };
    cmd->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    D3D12_VIEWPORT vp = { 0, 0, float(mipSize), float(mipSize), 0, 1 };
    D3D12_RECT     sc = { 0, 0, LONG(mipSize),  LONG(mipSize) };
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);
     
    const FaceDesc& fd = kFaces[faceIndex];
    XMVECTOR eye = XMVectorZero();
    XMVECTOR at = XMLoadFloat3(&fd.front);
    XMVECTOR up = XMLoadFloat3(&fd.up);
     
    XMMATRIX view = XMMatrixLookAtRH(eye, at, up);
    XMMATRIX proj = XMMatrixPerspectiveFovRH(XM_PIDIV2, 1.0f, 0.1f, 100.0f);
    XMMATRIX vp_m = view * proj;
     
    bool flipZ = (faceIndex == 0 || faceIndex == 1);
    bool flipX = !flipZ;

    m_faceCBPtr->roughness = roughness;
    m_faceCBPtr->flipX = flipX ? 1 : 0;
    m_faceCBPtr->flipZ = flipZ ? 1 : 0;
    XMStoreFloat4x4(&m_faceCBPtr->vp, XMMatrixTranspose(vp_m));
     
    ID3D12DescriptorHeap* heaps[] = { app->getShaderDescriptors()->getHeap() };
    cmd->SetDescriptorHeaps(1, heaps);

    cmd->SetGraphicsRootSignature(rs);
    cmd->SetPipelineState(pso);
    cmd->SetGraphicsRootConstantBufferView(0, m_faceCB->GetGPUVirtualAddress());
    cmd->SetGraphicsRootDescriptorTable(1, sourceSRV);
     
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 1, &m_vbView);
    cmd->DrawInstanced(36, 1, 0, 0);
     
    auto barrierOut = CD3DX12_RESOURCE_BARRIER::Transition(
        target,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        subRes);
    cmd->ResourceBarrier(1, &barrierOut);
}
 
bool IBLGenerator::generate(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmd,
    EnvironmentMap& env)
{
    LOG("IBLGenerator: starting IBL bake...");

    if (!env.isValid())
    {
        LOG("IBLGenerator: source environment map is not valid");
        return false;
    }

    if (!ensureGeometry(device)) return false;
    if (!ensureFaceCB(device))   return false;

    auto* shaderDescs = app->getShaderDescriptors();
     
    static constexpr uint32_t kIrradianceSize = 32;
    static constexpr uint32_t kPrefilterSize = 128;
    static constexpr uint32_t kBRDFLUTSize = 512;
    static constexpr uint32_t kNumRoughness = EnvironmentMap::NUM_ROUGHNESS_LEVELS;

    if (!createCubemapResource(device, kIrradianceSize, 1,
        DXGI_FORMAT_R16G16B16A16_FLOAT, L"IrradianceCubemap",
        env.irradianceCubemap)) return false;

    if (!createCubemapResource(device, kPrefilterSize, kNumRoughness,
        DXGI_FORMAT_R16G16B16A16_FLOAT, L"PrefilteredEnvCubemap",
        env.prefilteredCubemap)) return false;

    if (!create2DResource(device, kBRDFLUTSize,
        DXGI_FORMAT_R16G16_FLOAT, L"BRDFIntegrationLUT",
        env.brdfLUT)) return false;
     
    ComPtr<ID3D12RootSignature> irradianceRS, prefilterRS, brdfRS;
    ComPtr<ID3D12PipelineState> irradiancePSO, prefilterPSO, brdfPSO;

    if (!createConvPipeline(device, L"IrradianceMapPS.cso",
        DXGI_FORMAT_R16G16B16A16_FLOAT, irradianceRS, irradiancePSO))
    {
        LOG("IBLGenerator: failed to create irradiance pipeline"); return false;
    }

    if (!createConvPipeline(device, L"PrefilterEnvMapPS.cso",
        DXGI_FORMAT_R16G16B16A16_FLOAT, prefilterRS, prefilterPSO))
    {
        LOG("IBLGenerator: failed to create prefilter pipeline"); return false;
    }

    if (!createBRDFPipeline(device, brdfRS, brdfPSO))
    {
        LOG("IBLGenerator: failed to create BRDF pipeline"); return false;
    }
     
    LOG("IBLGenerator: baking irradiance map...");
    for (uint32_t face = 0; face < 6; ++face)
    {
        renderCubeFace(device, cmd,
            env.irradianceCubemap.Get(),
            face, 0, 1, kIrradianceSize, 0.0f,
            irradianceRS.Get(), irradiancePSO.Get(),
            env.srvTable.getGPUHandle(),
            DXGI_FORMAT_R16G16B16A16_FLOAT);
    }
     
    LOG("IBLGenerator: baking pre-filtered env map (%u roughness levels)...", kNumRoughness);
    for (uint32_t mip = 0; mip < kNumRoughness; ++mip)
    {
        float roughness = (kNumRoughness > 1)
            ? float(mip) / float(kNumRoughness - 1)
            : 0.0f;

        for (uint32_t face = 0; face < 6; ++face)
        {
            renderCubeFace(device, cmd,
                env.prefilteredCubemap.Get(),
                face, mip, kNumRoughness, kPrefilterSize, roughness,
                prefilterRS.Get(), prefilterPSO.Get(),
                env.srvTable.getGPUHandle(),
                DXGI_FORMAT_R16G16B16A16_FLOAT);
        }
    } 

    LOG("IBLGenerator: baking BRDF integration LUT...");
    {
        auto* rtDescs = app->getRTDescriptors();
         
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

        D3D12_VIEWPORT vp = { 0, 0, float(kBRDFLUTSize), float(kBRDFLUTSize), 0, 1 };
        D3D12_RECT     sc = { 0, 0, LONG(kBRDFLUTSize),  LONG(kBRDFLUTSize) };
        cmd->RSSetViewports(1, &vp);
        cmd->RSSetScissorRects(1, &sc);

        cmd->SetGraphicsRootSignature(brdfRS.Get());
        cmd->SetPipelineState(brdfPSO.Get());
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmd->IASetVertexBuffers(0, 0, nullptr);
        cmd->DrawInstanced(3, 1, 0, 0);    

        auto barrierOut = CD3DX12_RESOURCE_BARRIER::Transition(
            env.brdfLUT.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        cmd->ResourceBarrier(1, &barrierOut);
    }
      
    env.irradianceSRVTable = shaderDescs->allocTable("IBL_Irradiance");
    if (!env.irradianceSRVTable.isValid())
    {
        LOG("IBLGenerator: failed to alloc irradiance SRV table"); return false;
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
     
    env.prefilteredSRVTable = shaderDescs->allocTable("IBL_Prefilter");
    if (!env.prefilteredSRVTable.isValid())
    {
        LOG("IBLGenerator: failed to alloc prefilter SRV table"); return false;
    }
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.TextureCube.MipLevels = kNumRoughness;
        srvDesc.TextureCube.MostDetailedMip = 0;
        env.prefilteredSRVTable.createSRV(env.prefilteredCubemap.Get(), 0, &srvDesc);
    }
     
    env.brdfLUTSRVTable = shaderDescs->allocTable("IBL_BRDF_LUT");
    if (!env.brdfLUTSRVTable.isValid())
    {
        LOG("IBLGenerator: failed to alloc BRDF LUT SRV table"); return false;
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

    LOG("IBLGenerator: IBL bake complete.");
    return true;
}