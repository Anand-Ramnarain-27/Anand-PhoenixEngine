#include "Globals.h"
#include "HDRToCubemapPass.h"
#include "FaceProjection.h"
#include "D3D12ResourceFactory.h"
#include "CubemapPipelineBuilder.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleGPUResources.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "EnvironmentMap.h"
#include "ShaderTableDesc.h"
#include "CubeGeometry.h"
#include "ReadData.h"

#include <DirectXMath.h>
#include <array>
#include <algorithm>

using namespace DirectX;

HDRToCubemapPass::HDRToCubemapPass() = default;
HDRToCubemapPass::~HDRToCubemapPass() = default;

bool HDRToCubemapPass::loadHDRTexture(ID3D12Device* device, const std::string& hdrFile, EnvironmentMap& env) {
    auto* resources = app->getGPUResources();
    auto* shaderDescs = app->getShaderDescriptors();

    m_hdrTex = resources->createTextureFromFile(hdrFile, false);
    if (!m_hdrTex) {
        LOG("HDRToCubemapPass: failed to load HDR '%s'", hdrFile.c_str());
        return false;
    }

    m_hdrSRVTable = shaderDescs->allocTable("HDR_Equirect");
    if (!m_hdrSRVTable.isValid()) {
        LOG("HDRToCubemapPass: failed to alloc HDR SRV table");
        return false;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = m_hdrTex->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    m_hdrSRVTable.createSRV(m_hdrTex.Get(), 0, &srvDesc);

    if (!ensureGeometry(device)) return false;
    if (!ensureFaceCB(device)) return false;

    if (!m_convPSO && !createConversionPipeline(device)) return false;
    if (!m_mipPSO && !createMipPipeline(device)) return false;

    return true;
}

bool HDRToCubemapPass::createCubemapResource(ID3D12Device* device, EnvironmentMap& env, uint32_t cubeFaceSize) {
    m_cubeFaceSize = cubeFaceSize;
    m_numMips = 1;
    for (uint32_t s = cubeFaceSize; s > 1; s >>= 1) ++m_numMips;

    if (!D3D12ResourceFactory::createCubemapRT(device, cubeFaceSize, m_numMips, DXGI_FORMAT_R16G16B16A16_FLOAT, L"SkyboxCubemap", env.cubemap)) {
        LOG("HDRToCubemapPass: failed to create cubemap resource (size=%u mips=%u)", cubeFaceSize, m_numMips);
        return false;
    }

    LOG("HDRToCubemapPass: cubemap resource created (faceSize=%u mips=%u)", cubeFaceSize, m_numMips);
    return true;
}

bool HDRToCubemapPass::recordConversion(ID3D12GraphicsCommandList* cmd, EnvironmentMap& env) {
    ID3D12DescriptorHeap* heaps[] = { app->getShaderDescriptors()->getHeap() };
    cmd->SetDescriptorHeaps(1, heaps);

    for (uint32_t face = 0; face < 6; ++face) {
        renderFace(cmd, env.cubemap.Get(), face, 0, m_numMips, m_cubeFaceSize, m_hdrSRVTable.getGPUHandle(0), m_convRS.Get(), m_convPSO.Get(), DXGI_FORMAT_R16G16B16A16_FLOAT);
    }
    return true;
}

bool HDRToCubemapPass::recordMipLevel(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, EnvironmentMap& env, uint32_t mipIndex) {
    ID3D12DescriptorHeap* heaps[] = { app->getShaderDescriptors()->getHeap() };
    cmd->SetDescriptorHeaps(1, heaps);

    for (uint32_t face = 0; face < 6; ++face)
        blitMipFace(device, cmd, env.cubemap.Get(), face, mipIndex, m_numMips, m_cubeFaceSize);

    return true;
}

bool HDRToCubemapPass::finaliseSRV(EnvironmentMap& env) {
    auto* shaderDescs = app->getShaderDescriptors();
    env.srvTable = shaderDescs->allocTable("SkyboxCubemap");

    if (!env.srvTable.isValid()) {
        LOG("HDRToCubemapPass: failed to alloc cubemap SRV table");
        return false;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.TextureCube.MipLevels = m_numMips;
    srvDesc.TextureCube.MostDetailedMip = 0;
    env.srvTable.createSRV(env.cubemap.Get(), 0, &srvDesc);

    m_hdrTex.Reset();
    m_hdrSRVTable.reset();

    LOG("HDRToCubemapPass: cubemap SRV finalised (%u mips).", m_numMips);
    return true;
}

bool HDRToCubemapPass::createConversionPipeline(ID3D12Device* device) {
    if (!CubemapPipelineBuilder::buildCubeFacePipeline(device, L"HDRToCubemapPS.cso", DXGI_FORMAT_R16G16B16A16_FLOAT, m_convRS, m_convPSO)) {
        LOG("HDRToCubemapPass: failed to create conversion pipeline");
        return false;
    }
    return true;
}

bool HDRToCubemapPass::createMipPipeline(ID3D12Device* device) {
    ComPtr<ID3DBlob> blob, err;

    CD3DX12_DESCRIPTOR_RANGE srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER params[1];
    params[0].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
    rsDesc.Init(1, params, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_NONE);

    if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err))) {
        LOG("HDRToCubemapPass: mip RS serialize failed: %s", err ? (char*)err->GetBufferPointer() : "?");
        return false;
    }

    if (FAILED(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&m_mipRS))))
        return false;

    auto vs = DX::ReadData(L"FullScreenTriangleVS.cso");
    auto ps = DX::ReadData(L"MipChainPS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_mipRS.Get();
    psoDesc.VS = { vs.data(), vs.size() };
    psoDesc.PS = { ps.data(), ps.size() };
    psoDesc.InputLayout = { nullptr, 0 };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    psoDesc.NumRenderTargets = 1;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    if (FAILED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_mipPSO)))) {
        LOG("HDRToCubemapPass: failed to create mip PSO");
        return false;
    }

    return true;
}

bool HDRToCubemapPass::ensureGeometry(ID3D12Device* device) {
    if (m_geometryReady)
        return true;

    auto* resources = app->getGPUResources();
    m_cubeVB = resources->createDefaultBuffer(CubeGeometry::kCubeVerts, CubeGeometry::kCubeVertexSize, "HDR_CubeVB");

    if (!m_cubeVB) {
        LOG("HDRToCubemapPass: failed to create cube VB");
        return false;
    }

    m_vbView.BufferLocation = m_cubeVB->GetGPUVirtualAddress();
    m_vbView.StrideInBytes = CubeGeometry::kCubeVertexStride;
    m_vbView.SizeInBytes = CubeGeometry::kCubeVertexSize;
    m_geometryReady = true;
    return true;
}

bool HDRToCubemapPass::ensureFaceCB(ID3D12Device* device) {
    if (m_faceCB)
        return true;

    FaceCB zero{};
    m_faceCB = app->getGPUResources()->createUploadBuffer(&zero, (sizeof(FaceCB) + 255) & ~255u, "HDR_FaceCB");

    if (!m_faceCB) {
        LOG("HDRToCubemapPass: failed to create face CB");
        return false;
    }

    m_faceCB->Map(0, nullptr, reinterpret_cast<void**>(&m_faceCBPtr));
    return true;
}

void HDRToCubemapPass::renderFace(ID3D12GraphicsCommandList* cmd, ID3D12Resource* target, uint32_t faceIndex, uint32_t mipLevel, uint32_t totalMips, uint32_t baseFaceSize, D3D12_GPU_DESCRIPTOR_HANDLE sourceSRV, ID3D12RootSignature* rs, ID3D12PipelineState* pso, DXGI_FORMAT rtvFmt) {
    auto* rtDescs = app->getRTDescriptors();
    uint32_t mipSize = std::max(1u, baseFaceSize >> mipLevel);

    RenderTargetDesc rtv = rtDescs->create(target, faceIndex, mipLevel, rtvFmt);
    if (!rtv) {
        LOG("HDRToCubemapPass: RTV alloc failed (face=%u mip=%u)", faceIndex, mipLevel);
        return;
    }

    UINT subRes = D3D12CalcSubresource(mipLevel, faceIndex, 0, totalMips, 6);
    auto toRT = CD3DX12_RESOURCE_BARRIER::Transition(target, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET, subRes);
    cmd->ResourceBarrier(1, &toRT);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtv.getCPUHandle();
    cmd->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    float clear[4] = { 0, 0, 0, 1 };
    cmd->ClearRenderTargetView(rtvHandle, clear, 0, nullptr);

    D3D12_VIEWPORT vp = { 0, 0, float(mipSize), float(mipSize), 0, 1 };
    D3D12_RECT sc = { 0, 0, LONG(mipSize), LONG(mipSize) };
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);

    XMMATRIX vp_m = FaceProjection::viewProj(faceIndex);
    m_faceCBPtr->flipX = FaceProjection::needsFlipX(faceIndex) ? 1 : 0;
    m_faceCBPtr->flipZ = FaceProjection::needsFlipZ(faceIndex) ? 1 : 0;
    m_faceCBPtr->roughness = 0.0f;
    m_faceCBPtr->pad[0] = 0.0f;
    XMStoreFloat4x4(&m_faceCBPtr->vp, XMMatrixTranspose(vp_m));

    cmd->SetGraphicsRootSignature(rs);
    cmd->SetPipelineState(pso);
    cmd->SetGraphicsRootConstantBufferView(0, m_faceCB->GetGPUVirtualAddress());
    cmd->SetGraphicsRootDescriptorTable(1, sourceSRV);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 1, &m_vbView);
    cmd->DrawInstanced(36, 1, 0, 0);

    auto toSRV = CD3DX12_RESOURCE_BARRIER::Transition(target, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, subRes);
    cmd->ResourceBarrier(1, &toSRV);
}

void HDRToCubemapPass::blitMipFace(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, ID3D12Resource* cubemap, uint32_t faceIndex, uint32_t dstMip, uint32_t totalMips, uint32_t baseFaceSize) {
    auto* rtDescs = app->getRTDescriptors();
    auto* shaderDescs = app->getShaderDescriptors();
    uint32_t dstSize = std::max(1u, baseFaceSize >> dstMip);

    ShaderTableDesc srcTable = shaderDescs->allocTable("HDR_MipSrc");
    if (!srcTable.isValid()) {
        LOG("HDRToCubemapPass: mip SRV alloc failed (face=%u mip=%u)", faceIndex, dstMip);
        return;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2DArray.MostDetailedMip = dstMip - 1;
    srvDesc.Texture2DArray.MipLevels = 1;
    srvDesc.Texture2DArray.FirstArraySlice = faceIndex;
    srvDesc.Texture2DArray.ArraySize = 1;
    srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
    srcTable.createSRV(cubemap, 0, &srvDesc);

    RenderTargetDesc rtv = rtDescs->create(cubemap, faceIndex, dstMip, DXGI_FORMAT_R16G16B16A16_FLOAT);
    if (!rtv) {
        LOG("HDRToCubemapPass: mip RTV alloc failed (face=%u mip=%u)", faceIndex, dstMip);
        return;
    }

    UINT subResDst = D3D12CalcSubresource(dstMip, faceIndex, 0, totalMips, 6);
    auto toRT = CD3DX12_RESOURCE_BARRIER::Transition(cubemap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET, subResDst);
    cmd->ResourceBarrier(1, &toRT);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtv.getCPUHandle();
    cmd->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    D3D12_VIEWPORT vp = { 0, 0, float(dstSize), float(dstSize), 0, 1 };
    D3D12_RECT sc = { 0, 0, LONG(dstSize), LONG(dstSize) };
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);

    cmd->SetGraphicsRootSignature(m_mipRS.Get());
    cmd->SetPipelineState(m_mipPSO.Get());
    cmd->SetGraphicsRootDescriptorTable(0, srcTable.getGPUHandle(0));
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 0, nullptr);
    cmd->DrawInstanced(3, 1, 0, 0);

    auto toSRV = CD3DX12_RESOURCE_BARRIER::Transition(cubemap, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, subResDst);
    cmd->ResourceBarrier(1, &toSRV);
}