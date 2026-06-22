#include "Globals.h"
#include "ShadowMapPass.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleGPUResources.h"
#include "ModuleDSDescriptors.h"
#include "ModuleRTDescriptors.h"
#include "ModuleShaderDescriptors.h"
#include "GBuffer.h"
#include "ResourceMesh.h"
#include "Mesh.h"
#include "ReadData.h"
#include <d3dx12.h>

namespace {
    constexpr UINT cbAlign(UINT b){ return (b + 255u) & ~255u; }

    struct CubeMVP {
        Matrix worldLightViewProj;
        Matrix world;
        Vector3 lightPos;
        float invRange;
    };

    struct LightMatrixCB {
        Matrix invViewProj;
        Vector3 lightDir;
        float sunDistance;
    };
}

bool ShadowMapPipeline::init(ID3D12Device* device){
    return createRootSignature(device) && createPSO(device);
}

bool ShadowMapPipeline::createRootSignature(ID3D12Device* device){
    CD3DX12_ROOT_PARAMETER params[1];
    params[SLOT_MVP_CB].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(params), params, 0, nullptr,
              D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    if (FAILED(hr)){
        if (error) OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
        LOG("ShadowMapPipeline: serialize root sig failed 0x%08X", hr);
        return false;
    }
    hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                     IID_PPV_ARGS(&m_rootSig));
    if (FAILED(hr)){
        LOG("ShadowMapPipeline: CreateRootSignature failed 0x%08X", hr);
        return false;
    }
    return true;
}

bool ShadowMapPipeline::createPSO(ID3D12Device* device){
    auto vs = DX::ReadData(L"ShadowDepthVS.cso");
    auto ps = DX::ReadData(L"ShadowDepthPS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = m_rootSig.Get();
    desc.InputLayout = { Mesh::InputLayout, Mesh::InputLayoutCount };
    desc.VS = { vs.data(), vs.size() };
    desc.PS = { ps.data(), ps.size() };
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    desc.NumRenderTargets = 0;
    desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    desc.SampleDesc = { 1, 0 };
    desc.SampleMask = UINT_MAX;

    desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    desc.RasterizerState.FrontCounterClockwise = TRUE;
    desc.RasterizerState.DepthBias = 1000;
    desc.RasterizerState.SlopeScaledDepthBias = 1.5f;
    desc.RasterizerState.DepthBiasClamp = 0.0f;

    desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    HRESULT hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_pso));
    if (FAILED(hr)){
        LOG("ShadowMapPipeline: CreateGraphicsPipelineState failed 0x%08X", hr);
        return false;
    }
    return true;
}

bool ShadowMomentsPipeline::init(ID3D12Device* device){
    CD3DX12_ROOT_PARAMETER params[2];
    params[SLOT_MVP_CB].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
    params[SLOT_MOMENT_CONSTS].InitAsConstants(4, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(params), params, 0, nullptr,
              D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    if (FAILED(hr)){
        if (error) OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
        LOG("ShadowMomentsPipeline: serialize root sig failed 0x%08X", hr);
        return false;
    }
    hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                     IID_PPV_ARGS(&m_rootSig));
    if (FAILED(hr)){ LOG("ShadowMomentsPipeline: CreateRootSignature failed 0x%08X", hr); return false; }

    auto vs = DX::ReadData(L"ShadowDepthVS.cso");
    auto ps = DX::ReadData(L"ShadowMomentsPS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.pRootSignature = m_rootSig.Get();
    pso.InputLayout = { Mesh::InputLayout, Mesh::InputLayoutCount };
    pso.VS = { vs.data(), vs.size() };
    pso.PS = { ps.data(), ps.size() };
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R32G32_FLOAT;
    pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pso.SampleDesc = { 1, 0 };
    pso.SampleMask = UINT_MAX;
    pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    pso.RasterizerState.FrontCounterClockwise = TRUE;
    pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    hr = device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_pso));
    if (FAILED(hr)){ LOG("ShadowMomentsPipeline: CreatePSO failed 0x%08X", hr); return false; }
    return true;
}

bool ShadowCubePipeline::init(ID3D12Device* device){
    CD3DX12_ROOT_PARAMETER params[1];
    params[SLOT_MVP_CB].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(params), params, 0, nullptr,
              D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    if (FAILED(hr)){
        if (error) OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
        LOG("ShadowCubePipeline: serialize root sig failed 0x%08X", hr);
        return false;
    }
    hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                     IID_PPV_ARGS(&m_rootSig));
    if (FAILED(hr)){ LOG("ShadowCubePipeline: CreateRootSignature failed 0x%08X", hr); return false; }

    auto vs = DX::ReadData(L"ShadowCubeVS.cso");
    auto ps = DX::ReadData(L"ShadowCubePS.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.pRootSignature = m_rootSig.Get();
    pso.InputLayout = { Mesh::InputLayout, Mesh::InputLayoutCount };
    pso.VS = { vs.data(), vs.size() };
    pso.PS = { ps.data(), ps.size() };
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R32_FLOAT;
    pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pso.SampleDesc = { 1, 0 };
    pso.SampleMask = UINT_MAX;
    pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    pso.RasterizerState.FrontCounterClockwise = TRUE;
    pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    hr = device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_pso));
    if (FAILED(hr)){ LOG("ShadowCubePipeline: CreatePSO failed 0x%08X", hr); return false; }
    return true;
}

bool ShadowBlurPipeline::init(ID3D12Device* device){
    CD3DX12_DESCRIPTOR_RANGE inRange;  inRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE outRange; outRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

    CD3DX12_ROOT_PARAMETER params[3];
    params[SLOT_BLUR_CONSTS].InitAsConstants(4, 0);
    params[SLOT_INPUT].InitAsDescriptorTable(1, &inRange);
    params[SLOT_OUTPUT].InitAsDescriptorTable(1, &outRange);

    D3D12_STATIC_SAMPLER_DESC samp = {};
    samp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samp.AddressU = samp.AddressV = samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samp.MaxLOD = D3D12_FLOAT32_MAX;
    samp.ShaderRegister = 0;
    samp.RegisterSpace = 0;
    samp.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(params), params, 1, &samp, D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    if (FAILED(hr)){
        if (error) OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
        LOG("ShadowBlurPipeline: serialize root sig failed 0x%08X", hr);
        return false;
    }
    hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                     IID_PPV_ARGS(&m_rootSig));
    if (FAILED(hr)){ LOG("ShadowBlurPipeline: CreateRootSignature failed 0x%08X", hr); return false; }

    auto cs = DX::ReadData(L"ShadowBlurCS.cso");
    D3D12_COMPUTE_PIPELINE_STATE_DESC pso = {};
    pso.pRootSignature = m_rootSig.Get();
    pso.CS = { cs.data(), cs.size() };
    hr = device->CreateComputePipelineState(&pso, IID_PPV_ARGS(&m_pso));
    if (FAILED(hr)){ LOG("ShadowBlurPipeline: CreatePSO failed 0x%08X", hr); return false; }
    return true;
}

namespace {
    bool makeComputeRootSig(ID3D12Device* device, bool cbvNotConsts, UINT numConsts,
                            ComPtr<ID3D12RootSignature>& out){
        CD3DX12_DESCRIPTOR_RANGE inR;  inR.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
        CD3DX12_DESCRIPTOR_RANGE outR; outR.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
        CD3DX12_ROOT_PARAMETER p[3];
        if (cbvNotConsts) p[0].InitAsConstantBufferView(0);
        else              p[0].InitAsConstants(numConsts, 0);
        p[1].InitAsDescriptorTable(1, &inR);
        p[2].InitAsDescriptorTable(1, &outR);
        CD3DX12_ROOT_SIGNATURE_DESC desc;
        desc.Init(_countof(p), p, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);
        ComPtr<ID3DBlob> blob, err;
        HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err);
        if (FAILED(hr)){ if (err) OutputDebugStringA((char*)err->GetBufferPointer()); return false; }
        return SUCCEEDED(device->CreateRootSignature(0, blob->GetBufferPointer(),
                         blob->GetBufferSize(), IID_PPV_ARGS(&out)));
    }
    ComPtr<ID3D12PipelineState> makeCS(ID3D12Device* device, ID3D12RootSignature* rs,
                                       const wchar_t* cso){
        auto bc = DX::ReadData(cso);
        D3D12_COMPUTE_PIPELINE_STATE_DESC d = {};
        d.pRootSignature = rs;
        d.CS = { bc.data(), bc.size() };
        ComPtr<ID3D12PipelineState> pso;
        device->CreateComputePipelineState(&d, IID_PPV_ARGS(&pso));
        return pso;
    }
}

bool ShadowReducePipeline::init(ID3D12Device* device){
    if (!makeComputeRootSig(device, false, 4, m_rootSig)){
        LOG("ShadowReducePipeline: root sig failed"); return false;
    }
    m_initPso = makeCS(device, m_rootSig.Get(), L"ShadowReduceInitCS.cso");
    m_reducePso = makeCS(device, m_rootSig.Get(), L"ShadowReduceCS.cso");
    return m_initPso && m_reducePso;
}

bool ShadowLightMatrixPipeline::init(ID3D12Device* device){
    if (!makeComputeRootSig(device, true, 0, m_rootSig)){
        LOG("ShadowLightMatrixPipeline: root sig failed"); return false;
    }
    m_pso = makeCS(device, m_rootSig.Get(), L"ShadowLightMatrixCS.cso");
    return m_pso != nullptr;
}

bool ShadowDepthGpuPipeline::init(ID3D12Device* device){
    CD3DX12_ROOT_PARAMETER p[2];
    p[SLOT_WORLD_CB].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
    p[SLOT_VP_CB].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_VERTEX);
    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(p), p, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ComPtr<ID3DBlob> blob, err;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err);
    if (FAILED(hr)){ if (err) OutputDebugStringA((char*)err->GetBufferPointer());
                     LOG("ShadowDepthGpuPipeline: root sig failed"); return false; }
    if (FAILED(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                           IID_PPV_ARGS(&m_rootSig)))) return false;

    auto vs = DX::ReadData(L"ShadowDepthGpuVS.cso");
    auto ps = DX::ReadData(L"ShadowDepthPS.cso");
    D3D12_GRAPHICS_PIPELINE_STATE_DESC d = {};
    d.pRootSignature = m_rootSig.Get();
    d.InputLayout = { Mesh::InputLayout, Mesh::InputLayoutCount };
    d.VS = { vs.data(), vs.size() };
    d.PS = { ps.data(), ps.size() };
    d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    d.NumRenderTargets = 0;
    d.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    d.SampleDesc = { 1, 0 };
    d.SampleMask = UINT_MAX;
    d.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    d.RasterizerState.FrontCounterClockwise = TRUE;
    d.RasterizerState.DepthBias = 1000;
    d.RasterizerState.SlopeScaledDepthBias = 1.5f;
    d.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    d.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    return SUCCEEDED(device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&m_pso)));
}

bool ShadowMapPass::init(ID3D12Device* device){
    if (!m_pipeline.init(device)){
        LOG("ShadowMapPass: pipeline init failed");
        return false;
    }
    if (!m_momentPipeline.init(device)){
        LOG("ShadowMapPass: moment pipeline init failed");
        return false;
    }
    if (!m_cubePipeline.init(device)){
        LOG("ShadowMapPass: cube pipeline init failed");
        return false;
    }
    if (!m_blurPipeline.init(device)){
        LOG("ShadowMapPass: blur pipeline init failed");
        return false;
    }
    if (!m_reducePipeline.init(device) || !m_lightMatrixPipeline.init(device) ||
        !m_depthGpuPipeline.init(device)){
        LOG("ShadowMapPass: Phase 5 pipeline init failed");
        return false;
    }

    {
        auto hpDef = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer(256, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        if (FAILED(device->CreateCommittedResource(&hpDef, D3D12_HEAP_FLAG_NONE, &bd,
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, nullptr,
                IID_PPV_ARGS(&m_vpBuffer)))){
            LOG("ShadowMapPass: VP buffer create failed"); return false;
        }
        m_vpBuffer->SetName(L"ShadowMap_GpuVP");
        m_vpUav = app->getShaderDescriptors()->allocTable("ShadowMap_VPUav");
        D3D12_UNORDERED_ACCESS_VIEW_DESC u = {};
        u.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        u.Format = DXGI_FORMAT_UNKNOWN;
        u.Buffer.NumElements = 1;
        u.Buffer.StructureByteStride = (UINT)sizeof(Matrix);
        m_vpUav.createUAV(m_vpBuffer.Get(), 0, &u);
    }

    const UINT mvpSz = cbAlign(sizeof(Matrix));
    auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bd = CD3DX12_RESOURCE_DESC::Buffer((UINT64)mvpSz * MAX_DRAWS);
    HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                                                 D3D12_RESOURCE_STATE_GENERIC_READ,
                                                 nullptr, IID_PPV_ARGS(&m_mvpRing));
    if (FAILED(hr)){
        LOG("ShadowMapPass: MVP ring create failed 0x%08X", hr);
        return false;
    }
    m_mvpRing->SetName(L"ShadowMapPass_MVPRing");
    m_mvpRing->Map(0, nullptr, &m_mvpMapped);

    const UINT cubeSz = cbAlign(sizeof(CubeMVP));
    auto cbd = CD3DX12_RESOURCE_DESC::Buffer((UINT64)cubeSz * MAX_DRAWS);
    hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &cbd,
                                         D3D12_RESOURCE_STATE_GENERIC_READ,
                                         nullptr, IID_PPV_ARGS(&m_cubeRing));
    if (FAILED(hr)){
        LOG("ShadowMapPass: cube ring create failed 0x%08X", hr);
        return false;
    }
    m_cubeRing->SetName(L"ShadowMapPass_CubeRing");
    m_cubeRing->Map(0, nullptr, &m_cubeMapped);

    {
        const UINT sz = cbAlign(sizeof(LightMatrixCB));
        auto lbd = CD3DX12_RESOURCE_DESC::Buffer(sz);
        if (FAILED(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &lbd,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_lightMatrixCB)))){
            LOG("ShadowMapPass: light-matrix CB create failed"); return false;
        }
        m_lightMatrixCB->SetName(L"ShadowMap_LightMatrixCB");
        m_lightMatrixCB->Map(0, nullptr, &m_lightMatrixMapped);
    }

    if (!ensureResources(2048, 1)) return false;
    LOG("ShadowMapPass: init OK");
    return true;
}

bool ShadowMapPass::ensureResources(uint32_t resolution, int cascadeCount){
    if (resolution < 256) resolution = 256;
    if (resolution > 8192) resolution = 8192;
    if (cascadeCount < 1) cascadeCount = 1;
    if (cascadeCount > ShadowMath::kMaxCascades) cascadeCount = ShadowMath::kMaxCascades;
    if (m_depthTexture && m_resolution == resolution && m_cascadeCount == cascadeCount)
        return true;

    auto* gpuRes = app->getGPUResources();
    if (m_depthTexture){
        gpuRes->deferRelease(m_depthTexture);
        m_depthTexture.Reset();
        for (auto& d : m_dsvSlice) d.reset();
        m_srvTable.reset();
    }
    if (m_momentTex){
        gpuRes->deferRelease(m_momentTex);
        gpuRes->deferRelease(m_momentBlur);
        m_momentTex.Reset();
        m_momentBlur.Reset();
        for (auto& r : m_momentRtv) r.reset();
        m_momentSrv.reset(); m_momentUav.reset();
        m_blurSrv.reset(); m_blurUav.reset();
        m_momentReadable = false;
    }

    m_resolution = resolution;
    m_cascadeCount = cascadeCount;

    D3D12_RESOURCE_DESC dd = {};
    dd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    dd.Width = resolution;
    dd.Height = resolution;
    dd.DepthOrArraySize = (UINT16)cascadeCount;
    dd.MipLevels = 1;
    dd.Format = DXGI_FORMAT_R32_TYPELESS;
    dd.SampleDesc = { 1, 0 };
    dd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_CLEAR_VALUE cv = {};
    cv.Format = DXGI_FORMAT_D32_FLOAT;
    cv.DepthStencil.Depth = 1.0f;

    HRESULT hr = app->getD3D12()->getDevice()->CreateCommittedResource(
        &hp, D3D12_HEAP_FLAG_NONE, &dd, D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &cv, IID_PPV_ARGS(&m_depthTexture));
    if (FAILED(hr)){
        LOG("ShadowMapPass: depth texture create failed 0x%08X", hr);
        return false;
    }
    m_depthTexture->SetName(L"ShadowMap_DepthArray");
    m_readable = false;

    for (int i = 0; i < cascadeCount; ++i){
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.Texture2DArray.MipSlice = 0;
        dsvDesc.Texture2DArray.FirstArraySlice = (UINT)i;
        dsvDesc.Texture2DArray.ArraySize = 1;
        m_dsvSlice[i] = app->getDSDescriptors()->create(m_depthTexture.Get(), &dsvDesc);
    }

    m_srvTable = app->getShaderDescriptors()->allocTable("ShadowMap_SRV");
    if (m_srvTable.isValid()){
        D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
        srv.Format = DXGI_FORMAT_R32_FLOAT;
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Texture2DArray.MostDetailedMip = 0;
        srv.Texture2DArray.MipLevels = 1;
        srv.Texture2DArray.FirstArraySlice = 0;
        srv.Texture2DArray.ArraySize = (UINT)cascadeCount;
        m_srvTable.createSRV(m_depthTexture.Get(), 0, &srv);
    }

    return true;
}

void ShadowMapPass::render(ID3D12GraphicsCommandList* cmd,
                           const std::vector<MeshEntry*>& meshes,
                           const Matrix* viewProjs, int cascadeCount,
                           uint32_t resolution, int mode, float expK, float lightBleed){
    if (!ensureResources(resolution, cascadeCount)) return;
    cascadeCount = m_cascadeCount;

    BEGIN_EVENT(cmd, L"Shadow Map Pass");
    if (mode == 0){
        renderDepth(cmd, meshes, viewProjs, cascadeCount);
    } else {
        if (ensureMomentResources()){
            renderMoments(cmd, meshes, viewProjs, cascadeCount, mode, expK);
            blurMoments(cmd);
        }
    }
    END_EVENT(cmd);
}

void ShadowMapPass::renderDepth(ID3D12GraphicsCommandList* cmd,
                                const std::vector<MeshEntry*>& meshes,
                                const Matrix* viewProjs, int cascadeCount){
    if (m_readable){
        auto toDepth = CD3DX12_RESOURCE_BARRIER::Transition(
            m_depthTexture.Get(),
            D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_DEPTH_WRITE);
        cmd->ResourceBarrier(1, &toDepth);
        m_readable = false;
    }

    D3D12_VIEWPORT vp = { 0.f, 0.f, float(m_resolution), float(m_resolution), 0.f, 1.f };
    D3D12_RECT sc = { 0, 0, LONG(m_resolution), LONG(m_resolution) };
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);

    cmd->SetPipelineState(m_pipeline.getPSO());
    cmd->SetGraphicsRootSignature(m_pipeline.getRootSig());

    const UINT mvpSz = cbAlign(sizeof(Matrix));
    for (int c = 0; c < cascadeCount; ++c){
        D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_dsvSlice[c].getCPUHandle();
        cmd->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
        cmd->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        const Matrix& lightViewProj = viewProjs[c];
        for (MeshEntry* entry : meshes){
            if (!entry) continue;
            Mesh* mesh = entry->meshRes ? entry->meshRes->getMesh() : entry->mesh;
            if (!mesh) continue;
            if (m_ringCursor >= MAX_DRAWS) m_ringCursor = 0;
            const UINT slot = m_ringCursor++;

            Matrix world;
            memcpy(&world, entry->worldMatrix, sizeof(float) * 16);
            Matrix wvp = (world * lightViewProj).Transpose();
            memcpy(static_cast<char*>(m_mvpMapped) + (UINT64)slot * mvpSz, &wvp, sizeof(wvp));

            cmd->SetGraphicsRootConstantBufferView(
                ShadowMapPipeline::SLOT_MVP_CB,
                m_mvpRing->GetGPUVirtualAddress() + (UINT64)slot * mvpSz);

            if (entry->skinnedVA != 0) mesh->drawSkinned(cmd, entry->skinnedVA);
            else                       mesh->draw(cmd);
        }
    }

    auto toSrv = CD3DX12_RESOURCE_BARRIER::Transition(
        m_depthTexture.Get(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmd->ResourceBarrier(1, &toSrv);
    m_readable = true;
}

bool ShadowMapPass::ensureMomentResources(){
    if (m_momentTex) return true;

    auto* device = app->getD3D12()->getDevice();
    auto* sd = app->getShaderDescriptors();
    auto* rtd = app->getRTDescriptors();

    auto makeArray = [&](ComPtr<ID3D12Resource>& out, D3D12_RESOURCE_STATES state,
                         const wchar_t* name) -> bool {
        D3D12_RESOURCE_DESC dd = {};
        dd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        dd.Width = m_resolution;
        dd.Height = m_resolution;
        dd.DepthOrArraySize = (UINT16)m_cascadeCount;
        dd.MipLevels = 1;
        dd.Format = DXGI_FORMAT_R32G32_FLOAT;
        dd.SampleDesc = { 1, 0 };
        dd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET |
                   D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &dd,
                                                     state, nullptr, IID_PPV_ARGS(&out));
        if (FAILED(hr)){ LOG("ShadowMapPass: moment texture create failed 0x%08X", hr); return false; }
        out->SetName(name);
        return true;
    };

    if (!makeArray(m_momentTex, D3D12_RESOURCE_STATE_RENDER_TARGET, L"ShadowMap_Moments")) return false;
    if (!makeArray(m_momentBlur, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, L"ShadowMap_MomentsBlur")) return false;
    m_momentReadable = false;

    for (int i = 0; i < m_cascadeCount; ++i)
        m_momentRtv[i] = rtd->create(m_momentTex.Get(), (UINT)i, 0, DXGI_FORMAT_R32G32_FLOAT);

    auto arraySRV = [&](ShaderTableDesc& table, ID3D12Resource* res){
        table = sd->allocTable("ShadowMap_MomentSRV");
        D3D12_SHADER_RESOURCE_VIEW_DESC s = {};
        s.Format = DXGI_FORMAT_R32G32_FLOAT;
        s.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        s.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        s.Texture2DArray.MipLevels = 1;
        s.Texture2DArray.ArraySize = (UINT)m_cascadeCount;
        table.createSRV(res, 0, &s);
    };
    auto arrayUAV = [&](ShaderTableDesc& table, ID3D12Resource* res){
        table = sd->allocTable("ShadowMap_MomentUAV");
        D3D12_UNORDERED_ACCESS_VIEW_DESC u = {};
        u.Format = DXGI_FORMAT_R32G32_FLOAT;
        u.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
        u.Texture2DArray.ArraySize = (UINT)m_cascadeCount;
        table.createUAV(res, 0, &u);
    };
    arraySRV(m_momentSrv, m_momentTex.Get());
    arrayUAV(m_momentUav, m_momentTex.Get());
    arraySRV(m_blurSrv, m_momentBlur.Get());
    arrayUAV(m_blurUav, m_momentBlur.Get());
    return true;
}

void ShadowMapPass::renderMoments(ID3D12GraphicsCommandList* cmd,
                                  const std::vector<MeshEntry*>& meshes,
                                  const Matrix* viewProjs, int cascadeCount,
                                  int mode, float expK){
    if (m_readable){
        auto toDepth = CD3DX12_RESOURCE_BARRIER::Transition(
            m_depthTexture.Get(),
            D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_DEPTH_WRITE);
        cmd->ResourceBarrier(1, &toDepth);
        m_readable = false;
    }
    if (m_momentReadable){
        auto toRT = CD3DX12_RESOURCE_BARRIER::Transition(
            m_momentTex.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmd->ResourceBarrier(1, &toRT);
        m_momentReadable = false;
    }

    D3D12_VIEWPORT vp = { 0.f, 0.f, float(m_resolution), float(m_resolution), 0.f, 1.f };
    D3D12_RECT sc = { 0, 0, LONG(m_resolution), LONG(m_resolution) };
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);

    cmd->SetPipelineState(m_momentPipeline.getPSO());
    cmd->SetGraphicsRootSignature(m_momentPipeline.getRootSig());

    const uint32_t useExp = (mode == 2) ? 1u : 0u;
    uint32_t consts[4];
    memcpy(&consts[0], &expK, sizeof(float));
    consts[1] = useExp;
    consts[2] = consts[3] = 0;
    cmd->SetGraphicsRoot32BitConstants(ShadowMomentsPipeline::SLOT_MOMENT_CONSTS, 4, consts, 0);

    float farDepth = 1.0f;
    if (useExp) farDepth = exp2f(1.0f * expK);
    const float clearMoments[4] = { farDepth, farDepth * farDepth, 0.f, 0.f };

    const UINT mvpSz = cbAlign(sizeof(Matrix));
    for (int c = 0; c < cascadeCount; ++c){
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_momentRtv[c].getCPUHandle();
        D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_dsvSlice[c].getCPUHandle();
        cmd->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        cmd->ClearRenderTargetView(rtv, clearMoments, 0, nullptr);
        cmd->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        const Matrix& lightViewProj = viewProjs[c];
        for (MeshEntry* entry : meshes){
            if (!entry) continue;
            Mesh* mesh = entry->meshRes ? entry->meshRes->getMesh() : entry->mesh;
            if (!mesh) continue;
            if (m_ringCursor >= MAX_DRAWS) m_ringCursor = 0;
            const UINT slot = m_ringCursor++;

            Matrix world;
            memcpy(&world, entry->worldMatrix, sizeof(float) * 16);
            Matrix wvp = (world * lightViewProj).Transpose();
            memcpy(static_cast<char*>(m_mvpMapped) + (UINT64)slot * mvpSz, &wvp, sizeof(wvp));

            cmd->SetGraphicsRootConstantBufferView(
                ShadowMomentsPipeline::SLOT_MVP_CB,
                m_mvpRing->GetGPUVirtualAddress() + (UINT64)slot * mvpSz);

            if (entry->skinnedVA != 0) mesh->drawSkinned(cmd, entry->skinnedVA);
            else                       mesh->draw(cmd);
        }
    }
}

void ShadowMapPass::blurMoments(ID3D12GraphicsCommandList* cmd){
    ID3D12DescriptorHeap* heaps[] = { app->getShaderDescriptors()->getHeap() };
    cmd->SetDescriptorHeaps(1, heaps);
    cmd->SetPipelineState(m_blurPipeline.getPSO());
    cmd->SetComputeRootSignature(m_blurPipeline.getRootSig());

    const UINT groupsXY = (m_resolution + 7) / 8;
    auto dispatch = [&](int dirX, int dirY, ShaderTableDesc& inSrv, ShaderTableDesc& outUav){
        int consts[4] = { dirX, dirY, (int)m_resolution, (int)m_resolution };
        cmd->SetComputeRoot32BitConstants(ShadowBlurPipeline::SLOT_BLUR_CONSTS, 4, consts, 0);
        cmd->SetComputeRootDescriptorTable(ShadowBlurPipeline::SLOT_INPUT, inSrv.getGPUHandle(0));
        cmd->SetComputeRootDescriptorTable(ShadowBlurPipeline::SLOT_OUTPUT, outUav.getGPUHandle(0));
        cmd->Dispatch(groupsXY, groupsXY, (UINT)m_cascadeCount);
    };

    CD3DX12_RESOURCE_BARRIER toRead[] = {
        CD3DX12_RESOURCE_BARRIER::Transition(m_momentTex.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(m_momentBlur.Get(),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
    };
    cmd->ResourceBarrier(2, toRead);
    dispatch(1, 0, m_momentSrv, m_blurUav);

    CD3DX12_RESOURCE_BARRIER swap[] = {
        CD3DX12_RESOURCE_BARRIER::Transition(m_momentBlur.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(m_momentTex.Get(),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
    };
    cmd->ResourceBarrier(2, swap);
    dispatch(0, 1, m_blurSrv, m_momentUav);

    auto toSrv = CD3DX12_RESOURCE_BARRIER::Transition(m_momentTex.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmd->ResourceBarrier(1, &toSrv);
    m_momentReadable = true;
}

bool ShadowMapPass::ensureSpotResources(uint32_t resolution){
    if (resolution < 256) resolution = 256;
    if (resolution > 4096) resolution = 4096;
    if (m_spotDepth && m_spotRes == resolution) return true;

    auto* gpuRes = app->getGPUResources();
    if (m_spotDepth){ gpuRes->deferRelease(m_spotDepth); m_spotDepth.Reset();
                      m_spotDsv.reset(); m_spotSrv.reset(); }
    m_spotRes = resolution;

    D3D12_RESOURCE_DESC dd = {};
    dd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    dd.Width = resolution; dd.Height = resolution;
    dd.DepthOrArraySize = 1; dd.MipLevels = 1;
    dd.Format = DXGI_FORMAT_R32_TYPELESS;
    dd.SampleDesc = { 1, 0 };
    dd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_CLEAR_VALUE cv = {}; cv.Format = DXGI_FORMAT_D32_FLOAT; cv.DepthStencil.Depth = 1.0f;
    HRESULT hr = app->getD3D12()->getDevice()->CreateCommittedResource(
        &hp, D3D12_HEAP_FLAG_NONE, &dd, D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv,
        IID_PPV_ARGS(&m_spotDepth));
    if (FAILED(hr)){ LOG("ShadowMapPass: spot depth create failed 0x%08X", hr); return false; }
    m_spotDepth->SetName(L"ShadowMap_SpotDepth");
    m_spotReadable = false;

    D3D12_DEPTH_STENCIL_VIEW_DESC dv = {};
    dv.Format = DXGI_FORMAT_D32_FLOAT; dv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    m_spotDsv = app->getDSDescriptors()->create(m_spotDepth.Get(), &dv);
    m_spotSrv = app->getShaderDescriptors()->allocTable("ShadowMap_SpotSRV");
    if (m_spotSrv.isValid())
        m_spotSrv.createTexture2DSRV(m_spotDepth.Get(), 0, DXGI_FORMAT_R32_FLOAT, 1);
    return true;
}

void ShadowMapPass::renderSpot(ID3D12GraphicsCommandList* cmd,
                               const std::vector<MeshEntry*>& meshes,
                               const Matrix& spotViewProj, uint32_t resolution){
    if (!ensureSpotResources(resolution)) return;

    BEGIN_EVENT(cmd, L"Spot Shadow Pass");
    if (m_spotReadable){
        auto b = CD3DX12_RESOURCE_BARRIER::Transition(m_spotDepth.Get(),
            D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_DEPTH_WRITE);
        cmd->ResourceBarrier(1, &b);
        m_spotReadable = false;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_spotDsv.getCPUHandle();
    cmd->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
    cmd->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    D3D12_VIEWPORT vp = { 0.f, 0.f, float(m_spotRes), float(m_spotRes), 0.f, 1.f };
    D3D12_RECT sc = { 0, 0, LONG(m_spotRes), LONG(m_spotRes) };
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);

    cmd->SetPipelineState(m_pipeline.getPSO());
    cmd->SetGraphicsRootSignature(m_pipeline.getRootSig());

    const UINT mvpSz = cbAlign(sizeof(Matrix));
    for (MeshEntry* entry : meshes){
        if (!entry) continue;
        Mesh* mesh = entry->meshRes ? entry->meshRes->getMesh() : entry->mesh;
        if (!mesh) continue;
        if (m_ringCursor >= MAX_DRAWS) m_ringCursor = 0;
        const UINT slot = m_ringCursor++;

        Matrix world; memcpy(&world, entry->worldMatrix, sizeof(float) * 16);
        Matrix wvp = (world * spotViewProj).Transpose();
        memcpy(static_cast<char*>(m_mvpMapped) + (UINT64)slot * mvpSz, &wvp, sizeof(wvp));
        cmd->SetGraphicsRootConstantBufferView(ShadowMapPipeline::SLOT_MVP_CB,
            m_mvpRing->GetGPUVirtualAddress() + (UINT64)slot * mvpSz);
        if (entry->skinnedVA != 0) mesh->drawSkinned(cmd, entry->skinnedVA);
        else                       mesh->draw(cmd);
    }

    auto b = CD3DX12_RESOURCE_BARRIER::Transition(m_spotDepth.Get(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmd->ResourceBarrier(1, &b);
    m_spotReadable = true;
    END_EVENT(cmd);
}

bool ShadowMapPass::ensurePointResources(uint32_t resolution){
    if (resolution < 256) resolution = 256;
    if (resolution > 2048) resolution = 2048;
    if (m_pointCube && m_pointRes == resolution) return true;

    auto* gpuRes = app->getGPUResources();
    if (m_pointCube){
        gpuRes->deferRelease(m_pointCube); gpuRes->deferRelease(m_pointDepth);
        m_pointCube.Reset(); m_pointDepth.Reset();
        for (auto& r : m_pointRtv) r.reset();
        m_pointDsv.reset(); m_pointSrv.reset();
    }
    m_pointRes = resolution;

    auto* device = app->getD3D12()->getDevice();
    auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    D3D12_RESOURCE_DESC cd = {};
    cd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    cd.Width = resolution; cd.Height = resolution;
    cd.DepthOrArraySize = 6; cd.MipLevels = 1;
    cd.Format = DXGI_FORMAT_R32_FLOAT;
    cd.SampleDesc = { 1, 0 };
    cd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &cd,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&m_pointCube));
    if (FAILED(hr)){ LOG("ShadowMapPass: point cube create failed 0x%08X", hr); return false; }
    m_pointCube->SetName(L"ShadowMap_PointCube");
    m_pointReadable = true;

    D3D12_RESOURCE_DESC dd = {};
    dd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    dd.Width = resolution; dd.Height = resolution;
    dd.DepthOrArraySize = 1; dd.MipLevels = 1;
    dd.Format = DXGI_FORMAT_R32_TYPELESS;
    dd.SampleDesc = { 1, 0 };
    dd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    D3D12_CLEAR_VALUE cv = {}; cv.Format = DXGI_FORMAT_D32_FLOAT; cv.DepthStencil.Depth = 1.0f;
    hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &dd,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv, IID_PPV_ARGS(&m_pointDepth));
    if (FAILED(hr)){ LOG("ShadowMapPass: point depth create failed 0x%08X", hr); return false; }
    m_pointDepth->SetName(L"ShadowMap_PointDepth");

    auto* rtd = app->getRTDescriptors();
    for (int i = 0; i < 6; ++i)
        m_pointRtv[i] = rtd->create(m_pointCube.Get(), (UINT)i, 0, DXGI_FORMAT_R32_FLOAT);

    D3D12_DEPTH_STENCIL_VIEW_DESC dv = {};
    dv.Format = DXGI_FORMAT_D32_FLOAT; dv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    m_pointDsv = app->getDSDescriptors()->create(m_pointDepth.Get(), &dv);

    m_pointSrv = app->getShaderDescriptors()->allocTable("ShadowMap_PointSRV");
    if (m_pointSrv.isValid()){
        D3D12_SHADER_RESOURCE_VIEW_DESC s = {};
        s.Format = DXGI_FORMAT_R32_FLOAT;
        s.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        s.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        s.TextureCube.MipLevels = 1;
        m_pointSrv.createSRV(m_pointCube.Get(), 0, &s);
    }
    return true;
}

void ShadowMapPass::renderPoint(ID3D12GraphicsCommandList* cmd,
                                const std::vector<MeshEntry*>& meshes,
                                const Matrix faceViewProj[6], const Vector3& lightPos,
                                float range, uint32_t resolution){
    if (!ensurePointResources(resolution)) return;

    BEGIN_EVENT(cmd, L"Point Shadow Pass");
    if (m_pointReadable){
        auto b = CD3DX12_RESOURCE_BARRIER::Transition(m_pointCube.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmd->ResourceBarrier(1, &b);
        m_pointReadable = false;
    }

    D3D12_VIEWPORT vp = { 0.f, 0.f, float(m_pointRes), float(m_pointRes), 0.f, 1.f };
    D3D12_RECT sc = { 0, 0, LONG(m_pointRes), LONG(m_pointRes) };
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);

    cmd->SetPipelineState(m_cubePipeline.getPSO());
    cmd->SetGraphicsRootSignature(m_cubePipeline.getRootSig());

    const UINT cubeSz = cbAlign(sizeof(CubeMVP));
    const float invRange = (range > 1e-4f) ? 1.0f / range : 1.0f;
    const float farClear[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_pointDsv.getCPUHandle();

    for (int f = 0; f < 6; ++f){
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_pointRtv[f].getCPUHandle();
        cmd->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        cmd->ClearRenderTargetView(rtv, farClear, 0, nullptr);
        cmd->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        for (MeshEntry* entry : meshes){
            if (!entry) continue;
            Mesh* mesh = entry->meshRes ? entry->meshRes->getMesh() : entry->mesh;
            if (!mesh) continue;
            if (m_cubeCursor >= MAX_DRAWS) m_cubeCursor = 0;
            const UINT slot = m_cubeCursor++;

            Matrix world; memcpy(&world, entry->worldMatrix, sizeof(float) * 16);
            CubeMVP c;
            c.worldLightViewProj = (world * faceViewProj[f]).Transpose();
            c.world = world.Transpose();
            c.lightPos = lightPos;
            c.invRange = invRange;
            memcpy(static_cast<char*>(m_cubeMapped) + (UINT64)slot * cubeSz, &c, sizeof(c));
            cmd->SetGraphicsRootConstantBufferView(ShadowCubePipeline::SLOT_MVP_CB,
                m_cubeRing->GetGPUVirtualAddress() + (UINT64)slot * cubeSz);

            if (entry->skinnedVA != 0) mesh->drawSkinned(cmd, entry->skinnedVA);
            else                       mesh->draw(cmd);
        }
    }

    auto b = CD3DX12_RESOURCE_BARRIER::Transition(m_pointCube.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmd->ResourceBarrier(1, &b);
    m_pointReadable = true;
    END_EVENT(cmd);
}

bool ShadowMapPass::ensureReduceResources(uint32_t depthW, uint32_t depthH){
    const uint32_t w = (depthW + 7) / 8;
    const uint32_t h = (depthH + 7) / 8;
    if (m_reduceA && m_reduceW == w && m_reduceH == h) return true;

    auto* gpuRes = app->getGPUResources();
    if (m_reduceA){ gpuRes->deferRelease(m_reduceA); gpuRes->deferRelease(m_reduceB);
                    m_reduceA.Reset(); m_reduceB.Reset();
                    m_reduceSrvA.reset(); m_reduceUavA.reset();
                    m_reduceSrvB.reset(); m_reduceUavB.reset(); }
    m_reduceW = w; m_reduceH = h;

    auto* device = app->getD3D12()->getDevice();
    auto* sd = app->getShaderDescriptors();
    auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto make = [&](ComPtr<ID3D12Resource>& out, const wchar_t* name){
        D3D12_RESOURCE_DESC dd = {};
        dd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        dd.Width = w; dd.Height = h; dd.DepthOrArraySize = 1; dd.MipLevels = 1;
        dd.Format = DXGI_FORMAT_R32G32_FLOAT; dd.SampleDesc = { 1, 0 };
        dd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        if (FAILED(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &dd,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&out))))
            return false;
        out->SetName(name);
        return true;
    };
    if (!make(m_reduceA, L"ShadowMap_ReduceA") || !make(m_reduceB, L"ShadowMap_ReduceB"))
        return false;

    auto srv = [&](ShaderTableDesc& t, ID3D12Resource* r){
        t = sd->allocTable("ShadowReduceSRV");
        D3D12_SHADER_RESOURCE_VIEW_DESC s = {};
        s.Format = DXGI_FORMAT_R32G32_FLOAT; s.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        s.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        s.Texture2D.MipLevels = 1; t.createSRV(r, 0, &s);
    };
    auto uav = [&](ShaderTableDesc& t, ID3D12Resource* r){
        t = sd->allocTable("ShadowReduceUAV");
        D3D12_UNORDERED_ACCESS_VIEW_DESC u = {};
        u.Format = DXGI_FORMAT_R32G32_FLOAT; u.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        t.createUAV(r, 0, &u);
    };
    srv(m_reduceSrvA, m_reduceA.Get()); uav(m_reduceUavA, m_reduceA.Get());
    srv(m_reduceSrvB, m_reduceB.Get()); uav(m_reduceUavB, m_reduceB.Get());
    return true;
}

bool ShadowMapPass::computeGpuLightMatrix(ID3D12GraphicsCommandList* cmd, GBuffer& gbuffer,
                                          const Matrix& invViewProj, const Vector3& lightDir,
                                          float sunDistance){
    if (!gbuffer.isValid() || !gbuffer.isDepthReadable()) return false;
    const uint32_t depthW = gbuffer.getWidth(), depthH = gbuffer.getHeight();
    if (!ensureReduceResources(depthW, depthH)) return false;

    BEGIN_EVENT(cmd, L"GPU Light Matrix (parallel reduction)");

    ID3D12DescriptorHeap* heaps[] = { app->getShaderDescriptors()->getHeap() };
    cmd->SetDescriptorHeaps(1, heaps);

    auto depthTex = gbuffer.getDepthTexture();
    auto toRead = CD3DX12_RESOURCE_BARRIER::Transition(depthTex,
        D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    cmd->ResourceBarrier(1, &toRead);

    cmd->SetComputeRootSignature(m_reducePipeline.getRootSig());

    auto uavToSrv = [&](ID3D12Resource* r){
        auto b = CD3DX12_RESOURCE_BARRIER::Transition(r,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        cmd->ResourceBarrier(1, &b);
    };
    auto srvToUav = [&](ID3D12Resource* r){
        auto b = CD3DX12_RESOURCE_BARRIER::Transition(r,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmd->ResourceBarrier(1, &b);
    };

    cmd->SetPipelineState(m_reducePipeline.getInitPSO());
    uint32_t dstW = (depthW + 7) / 8, dstH = (depthH + 7) / 8;
    {
        UINT cb[4] = { depthW, depthH, dstW, dstH };
        cmd->SetComputeRoot32BitConstants(ShadowReducePipeline::SLOT_CB, 4, cb, 0);
        cmd->SetComputeRootDescriptorTable(ShadowReducePipeline::SLOT_INPUT, gbuffer.getDepthSrvHandle());
        srvToUav(m_reduceA.Get());
        cmd->SetComputeRootDescriptorTable(ShadowReducePipeline::SLOT_OUTPUT, m_reduceUavA.getGPUHandle(0));
        cmd->Dispatch((dstW + 7) / 8, (dstH + 7) / 8, 1);
        uavToSrv(m_reduceA.Get());
    }

    cmd->SetPipelineState(m_reducePipeline.getReducePSO());
    uint32_t curW = dstW, curH = dstH;
    bool resultInA = true;
    while (curW > 1 || curH > 1){
        uint32_t nW = (curW + 7) / 8, nH = (curH + 7) / 8;
        ShaderTableDesc& srcSrv = resultInA ? m_reduceSrvA : m_reduceSrvB;
        ShaderTableDesc& dstUav = resultInA ? m_reduceUavB : m_reduceUavA;
        ID3D12Resource* dstRes = resultInA ? m_reduceB.Get() : m_reduceA.Get();
        UINT cb[4] = { curW, curH, nW, nH };
        cmd->SetComputeRoot32BitConstants(ShadowReducePipeline::SLOT_CB, 4, cb, 0);
        cmd->SetComputeRootDescriptorTable(ShadowReducePipeline::SLOT_INPUT, srcSrv.getGPUHandle(0));
        srvToUav(dstRes);
        cmd->SetComputeRootDescriptorTable(ShadowReducePipeline::SLOT_OUTPUT, dstUav.getGPUHandle(0));
        cmd->Dispatch((nW + 7) / 8, (nH + 7) / 8, 1);
        uavToSrv(dstRes);
        resultInA = !resultInA; curW = nW; curH = nH;
    }

    LightMatrixCB lcb;
    lcb.invViewProj = invViewProj.Transpose();
    lcb.lightDir = lightDir;
    lcb.sunDistance = sunDistance;
    memcpy(m_lightMatrixMapped, &lcb, sizeof(lcb));

    cmd->SetComputeRootSignature(m_lightMatrixPipeline.getRootSig());
    cmd->SetPipelineState(m_lightMatrixPipeline.getPSO());
    auto vpToUav = CD3DX12_RESOURCE_BARRIER::Transition(m_vpBuffer.Get(),
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    cmd->ResourceBarrier(1, &vpToUav);
    cmd->SetComputeRootConstantBufferView(ShadowLightMatrixPipeline::SLOT_CB,
        m_lightMatrixCB->GetGPUVirtualAddress());
    cmd->SetComputeRootDescriptorTable(ShadowLightMatrixPipeline::SLOT_INPUT,
        (resultInA ? m_reduceSrvA : m_reduceSrvB).getGPUHandle(0));
    cmd->SetComputeRootDescriptorTable(ShadowLightMatrixPipeline::SLOT_OUTPUT, m_vpUav.getGPUHandle(0));
    cmd->Dispatch(1, 1, 1);
    auto vpToCb = CD3DX12_RESOURCE_BARRIER::Transition(m_vpBuffer.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    cmd->ResourceBarrier(1, &vpToCb);

    auto restore = CD3DX12_RESOURCE_BARRIER::Transition(depthTex,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmd->ResourceBarrier(1, &restore);

    m_vpReady = true;
    END_EVENT(cmd);
    return true;
}

void ShadowMapPass::renderDirectionalGpu(ID3D12GraphicsCommandList* cmd,
                                         const std::vector<MeshEntry*>& meshes,
                                         uint32_t resolution){
    if (!m_vpReady) return;
    if (!ensureResources(resolution, 1)) return;

    BEGIN_EVENT(cmd, L"Shadow Map Pass (GPU VP)");
    if (m_readable){
        auto b = CD3DX12_RESOURCE_BARRIER::Transition(m_depthTexture.Get(),
            D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_DEPTH_WRITE);
        cmd->ResourceBarrier(1, &b);
        m_readable = false;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_dsvSlice[0].getCPUHandle();
    cmd->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
    cmd->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    D3D12_VIEWPORT vp = { 0.f, 0.f, float(m_resolution), float(m_resolution), 0.f, 1.f };
    D3D12_RECT sc = { 0, 0, LONG(m_resolution), LONG(m_resolution) };
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);

    cmd->SetPipelineState(m_depthGpuPipeline.getPSO());
    cmd->SetGraphicsRootSignature(m_depthGpuPipeline.getRootSig());
    cmd->SetGraphicsRootConstantBufferView(ShadowDepthGpuPipeline::SLOT_VP_CB,
        m_vpBuffer->GetGPUVirtualAddress());

    const UINT mvpSz = cbAlign(sizeof(Matrix));
    for (MeshEntry* entry : meshes){
        if (!entry) continue;
        Mesh* mesh = entry->meshRes ? entry->meshRes->getMesh() : entry->mesh;
        if (!mesh) continue;
        if (m_ringCursor >= MAX_DRAWS) m_ringCursor = 0;
        const UINT slot = m_ringCursor++;

        Matrix world; memcpy(&world, entry->worldMatrix, sizeof(float) * 16);
        Matrix wt = world.Transpose();
        memcpy(static_cast<char*>(m_mvpMapped) + (UINT64)slot * mvpSz, &wt, sizeof(wt));
        cmd->SetGraphicsRootConstantBufferView(ShadowDepthGpuPipeline::SLOT_WORLD_CB,
            m_mvpRing->GetGPUVirtualAddress() + (UINT64)slot * mvpSz);

        if (entry->skinnedVA != 0) mesh->drawSkinned(cmd, entry->skinnedVA);
        else                       mesh->draw(cmd);
    }

    auto toSrv = CD3DX12_RESOURCE_BARRIER::Transition(m_depthTexture.Get(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmd->ResourceBarrier(1, &toSrv);
    m_readable = true;
    END_EVENT(cmd);
}
