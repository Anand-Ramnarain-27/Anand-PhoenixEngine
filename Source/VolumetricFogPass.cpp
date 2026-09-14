#include "Globals.h"
#include "VolumetricFogPass.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "GBufferPass.h"
#include "GBuffer.h"
#include "ModuleSamplerHeap.h"
#include "ModuleShaderDescriptors.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "RenderTexture.h"
#include "ReadData.h"
#include <d3dx12.h>

namespace {
    constexpr UINT cbAlign(UINT b){ return (b + 255u) & ~255u; }

    ComPtr<ID3D12Resource> makeUploadBuf(ID3D12Device* device, UINT64 bytes, void** mapped, const wchar_t* name){
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer(bytes);
        ComPtr<ID3D12Resource> buf;
        HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                                                      D3D12_RESOURCE_STATE_GENERIC_READ,
                                                      nullptr, IID_PPV_ARGS(&buf));
        if (FAILED(hr)){ LOG("VolumetricFogPass: buf create failed 0x%08X", hr); return nullptr; }
        buf->SetName(name);
        if (mapped) buf->Map(0, nullptr, mapped);
        return buf;
    }
}

bool VolumetricFogPass::init(ID3D12Device* device){
    m_device = device;
    if (!createComputeRootSignature(device)){
        LOG("VolumetricFogPass: compute root signature creation failed");
        return false;
    }
    if (!createComputePSO(device)){
        LOG("VolumetricFogPass: compute PSO creation failed");
        return false;
    }
    if (!createCompositeRootSignature(device)){
        LOG("VolumetricFogPass: composite root signature creation failed");
        return false;
    }
    if (!createCompositePSO(device)){
        LOG("VolumetricFogPass: composite PSO creation failed");
        return false;
    }
    if (!createUploadBuffers(device)){
        LOG("VolumetricFogPass: upload buffer creation failed");
        return false;
    }
    LOG("VolumetricFogPass: init OK");
    return true;
}

bool VolumetricFogPass::createComputeRootSignature(ID3D12Device* device){
    CD3DX12_DESCRIPTOR_RANGE depthRange;
    depthRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_DESCRIPTOR_RANGE outRange;
    outRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

    CD3DX12_ROOT_PARAMETER params[3];
    params[0].InitAsConstantBufferView(0, 0);
    params[1].InitAsDescriptorTable(1, &depthRange);
    params[2].InitAsDescriptorTable(1, &outRange);

    CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
    rsDesc.Init(_countof(params), params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ComPtr<ID3DBlob> blob, err;
    if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err))){
        LOG("VolumetricFogPass: compute root signature serialise failed: %s", err ? (char*)err->GetBufferPointer() : "unknown error");
        return false;
    }
    return SUCCEEDED(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&m_computeRootSig)));
}

bool VolumetricFogPass::createComputePSO(ID3D12Device* device){
    auto cs = DX::ReadData(L"VolumetricFogCS.cso");

    D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = m_computeRootSig.Get();
    desc.CS = { cs.data(), cs.size() };

    return SUCCEEDED(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&m_computePSO)));
}

bool VolumetricFogPass::createCompositeRootSignature(ID3D12Device* device){
    CD3DX12_DESCRIPTOR_RANGE sceneColorRange;
    sceneColorRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_DESCRIPTOR_RANGE fogRange;
    fogRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

    CD3DX12_DESCRIPTOR_RANGE sampRange;
    sampRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, ModuleSamplerHeap::COUNT, 0);

    CD3DX12_ROOT_PARAMETER params[4];
    params[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    params[1].InitAsDescriptorTable(1, &sceneColorRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[2].InitAsDescriptorTable(1, &fogRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[3].InitAsDescriptorTable(1, &sampRange, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC rsDesc;
    rsDesc.Init(_countof(params), params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ComPtr<ID3DBlob> blob, err;
    if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err))){
        LOG("VolumetricFogPass: composite root signature serialise failed: %s", err ? (char*)err->GetBufferPointer() : "unknown error");
        return false;
    }
    return SUCCEEDED(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&m_compositeRootSig)));
}

bool VolumetricFogPass::createCompositePSO(ID3D12Device* device){
    auto vs = DX::ReadData(L"FullScreenVS.cso");
    auto ps = DX::ReadData(L"VolumetricFogCompositePS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = m_compositeRootSig.Get();
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

    return SUCCEEDED(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_compositePSO)));
}

bool VolumetricFogPass::createUploadBuffers(ID3D12Device* device){
    const UINT computeCbSz = cbAlign(sizeof(CbCompute));
    const UINT compositeCbSz = cbAlign(sizeof(CbComposite));
    for (int i = 0; i < NUM_VIEWPORTS; ++i){
        m_computeCB[i] = makeUploadBuf(device, computeCbSz, &m_computeMapped[i], L"VolumetricFog_ComputeCB");
        m_compositeCB[i] = makeUploadBuf(device, compositeCbSz, &m_compositeMapped[i], L"VolumetricFog_CompositeCB");
        if (!m_computeCB[i] || !m_compositeCB[i]) return false;
    }
    return true;
}

bool VolumetricFogPass::ensureFogTexture(uint32_t width, uint32_t height, int viewportIndex){
    if (m_fogTex[viewportIndex] && m_fogTexWidth[viewportIndex] == width && m_fogTexHeight[viewportIndex] == height)
        return true;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.SampleDesc = { 1, 0 };
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ComPtr<ID3D12Resource> tex;
    HRESULT hr = m_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &desc,
                                                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                                    nullptr, IID_PPV_ARGS(&tex));
    if (FAILED(hr)){
        LOG("VolumetricFogPass: fog texture create failed 0x%08X", hr);
        return false;
    }
    tex->SetName(L"VolumetricFog_Tex");

    auto* sd = app->getShaderDescriptors();
    m_fogSrv[viewportIndex] = sd->allocTable("VolumetricFog_SRV");
    m_fogUav[viewportIndex] = sd->allocTable("VolumetricFog_UAV");
    if (!m_fogSrv[viewportIndex].isValid() || !m_fogUav[viewportIndex].isValid()){
        LOG("VolumetricFogPass: fog texture SRV/UAV alloc failed");
        return false;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MipLevels = 1;
    m_fogSrv[viewportIndex].createSRV(tex.Get(), 0, &srv);

    D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {};
    uav.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    m_fogUav[viewportIndex].createUAV(tex.Get(), 0, &uav);

    m_fogTex[viewportIndex] = tex;
    m_fogTexWidth[viewportIndex] = width;
    m_fogTexHeight[viewportIndex] = height;
    return true;
}

RenderTexture* VolumetricFogPass::render(ID3D12GraphicsCommandList* cmd,
                                         RenderTexture* input,
                                         RenderTexture* output,
                                         GBufferPass& gbufferPass,
                                         const Vector3& cameraPos,
                                         const Matrix& invViewProj,
                                         float elapsedTime,
                                         const VolumetricFogSettings& settings,
                                         int viewportIndex){
    if (!settings.enabled) return input;
    if (!input || !input->isValid() || !output || !output->isValid()) return input;
    GBuffer& gb = gbufferPass.getGBuffer();
    if (!gb.isValid()) return input;

    viewportIndex = (viewportIndex >= 0 && viewportIndex < NUM_VIEWPORTS) ? viewportIndex : 0;

    const uint32_t width = input->getWidth();
    const uint32_t height = input->getHeight();
    if (width == 0 || height == 0) return input;
    if (!ensureFogTexture(width, height, viewportIndex)) return input;

    BEGIN_EVENT(cmd, L"Volumetric Fog");

    CbCompute cbc = {};
    cbc.invViewProj = invViewProj.Transpose();
    cbc.cameraPosition = cameraPos;
    cbc.time = elapsedTime;
    cbc.viewportWidth = width;
    cbc.viewportHeight = height;
    cbc.numSteps = settings.numSteps;
    cbc.extinctionCoeff = settings.extinctionCoeff;
    cbc.noiseAmount = settings.noiseAmount;
    cbc.fogIntensity = settings.fogIntensity;
    memcpy(m_computeMapped[viewportIndex], &cbc, sizeof(cbc));

    ID3D12DescriptorHeap* heaps[] = { app->getShaderDescriptors()->getHeap(), app->getSamplerHeap()->getHeap() };
    cmd->SetDescriptorHeaps(2, heaps);

    // GBuffer depth normally only carries PIXEL_SHADER_RESOURCE visibility (for the deferred
    // lighting pass); add NON_PIXEL_SHADER_RESOURCE so this compute pass can read it too, then
    // restore the exact state GBufferPass expects at the start of next frame.
    ID3D12Resource* depthTex = gb.getDepthTexture();
    auto toCompute = CD3DX12_RESOURCE_BARRIER::Transition(depthTex,
        D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    cmd->ResourceBarrier(1, &toCompute);

    cmd->SetPipelineState(m_computePSO.Get());
    cmd->SetComputeRootSignature(m_computeRootSig.Get());
    cmd->SetComputeRootConstantBufferView(0, m_computeCB[viewportIndex]->GetGPUVirtualAddress());
    cmd->SetComputeRootDescriptorTable(1, gb.getDepthSrvHandle());
    cmd->SetComputeRootDescriptorTable(2, m_fogUav[viewportIndex].getGPUHandle(0));

    const UINT groupsX = (width + 7) / 8;
    const UINT groupsY = (height + 7) / 8;
    cmd->Dispatch(groupsX, groupsY, 1);

    auto fromCompute = CD3DX12_RESOURCE_BARRIER::Transition(depthTex,
        D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmd->ResourceBarrier(1, &fromCompute);

    auto uavToSrv = CD3DX12_RESOURCE_BARRIER::Transition(m_fogTex[viewportIndex].Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmd->ResourceBarrier(1, &uavToSrv);

    CbComposite cbComp = {};
    cbComp.maxOpacity = settings.maxOpacity;
    memcpy(m_compositeMapped[viewportIndex], &cbComp, sizeof(cbComp));

    output->beginRender(cmd, false);
    cmd->SetGraphicsRootSignature(m_compositeRootSig.Get());
    cmd->SetPipelineState(m_compositePSO.Get());
    cmd->SetGraphicsRootConstantBufferView(0, m_compositeCB[viewportIndex]->GetGPUVirtualAddress());
    cmd->SetGraphicsRootDescriptorTable(1, input->getSrvHandle());
    cmd->SetGraphicsRootDescriptorTable(2, m_fogSrv[viewportIndex].getGPUHandle(0));
    cmd->SetGraphicsRootDescriptorTable(3, app->getSamplerHeap()->getGPUHandle(ModuleSamplerHeap::LINEAR_WRAP));
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 0, nullptr);
    cmd->DrawInstanced(3, 1, 0, 0);
    output->endRender(cmd);

    auto srvToUav = CD3DX12_RESOURCE_BARRIER::Transition(m_fogTex[viewportIndex].Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    cmd->ResourceBarrier(1, &srvToUav);

    END_EVENT(cmd);

    return output;
}
