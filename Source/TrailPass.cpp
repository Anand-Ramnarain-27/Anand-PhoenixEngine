#include "Globals.h"
#include "TrailPass.h"
#include "Application.h"
#include "ModuleD3D12.h"
#include "ModuleGPUResources.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleSamplerHeap.h"
#include "ModuleEditor.h"
#include <imgui.h>
#include <d3dx12.h>
#include <algorithm>
#include <cstring>

namespace {
    constexpr UINT cbAlign(UINT b){ return (b + 255u) & ~255u; }
}

bool TrailPass::init(ID3D12Device* device){
    if (!m_pipeline.init(device)){
        LOG("TrailPass: pipeline init failed");
        return false;
    }
    if (!createBuffers(device)) return false;
    if (!createFallbackTexture(device)) return false;

    LOG("TrailPass: init OK");
    if (auto* ed = app->getEditor())
        ed->log("TrailPass: initialized OK", ImVec4(0.5f, 1.f, 0.5f, 1.f));
    return true;
}

bool TrailPass::createBuffers(ID3D12Device* device){
    m_vbStride = sizeof(ComponentTrail::TrailVertex);
    const UINT64 vbTotal = (UINT64)m_vbStride * MAX_TRAIL_VERTICES;
    {
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer(vbTotal);
        HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                                                      D3D12_RESOURCE_STATE_GENERIC_READ,
                                                      nullptr, IID_PPV_ARGS(&m_vbRing));
        if (FAILED(hr)){ LOG("TrailPass: VB ring alloc failed 0x%08X", hr); return false; }
        m_vbRing->SetName(L"Trail_VBRing");
        m_vbRing->Map(0, nullptr, &m_vbMapped);
    }
    {
        const UINT stride = cbAlign(sizeof(TrailInstanceCB));
        const UINT64 total = (UINT64)stride * MAX_TRAILS * 2; // *2 for Scene View + Game View in same frame
        auto hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bd = CD3DX12_RESOURCE_DESC::Buffer(total);
        HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd,
                                                      D3D12_RESOURCE_STATE_GENERIC_READ,
                                                      nullptr, IID_PPV_ARGS(&m_cbRing));
        if (FAILED(hr)){ LOG("TrailPass: CB ring alloc failed 0x%08X", hr); return false; }
        m_cbRing->SetName(L"Trail_CBRing");
        m_cbRing->Map(0, nullptr, &m_cbMapped);
    }
    return true;
}

bool TrailPass::createFallbackTexture(ID3D12Device* device){
    (void)device;
    const uint32_t white = 0xFFFFFFFFu;
    m_fallbackTex = app->getGPUResources()->createRawTexture2D(&white, sizeof(white), 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
    if (!m_fallbackTex){
        LOG("TrailPass: fallback texture creation failed");
        return false;
    }
    m_fallbackTex->SetName(L"Trail_FallbackTex");

    auto* sd = app->getShaderDescriptors();
    m_fallbackSRV = sd->allocTable("Trail_FallbackSRV");
    if (!m_fallbackSRV.isValid()){
        LOG("TrailPass: fallback SRV alloc failed");
        return false;
    }
    D3D12_SHADER_RESOURCE_VIEW_DESC sv = {};
    sv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sv.Texture2D.MipLevels = 1;
    m_fallbackSRV.createSRV(m_fallbackTex.Get(), 0, &sv);
    return true;
}

D3D12_GPU_DESCRIPTOR_HANDLE TrailPass::getOrLoadTexture(const std::string& path){
    if (path.empty())
        return m_fallbackSRV.getGPUHandle(0);

    auto it = m_textureCache.find(path);
    if (it != m_textureCache.end())
        return it->second.srv.getGPUHandle(0);

    auto* gpu = app->getGPUResources();
    ComPtr<ID3D12Resource> tex = gpu ? gpu->createTextureFromFile(path, true) : nullptr;
    if (!tex){
        LOG("TrailPass: failed to load texture '%s', using fallback", path.c_str());
        m_textureCache.emplace(path, CachedTexture{ nullptr, m_fallbackSRV });
        return m_fallbackSRV.getGPUHandle(0);
    }

    ShaderTableDesc srv = app->getShaderDescriptors()->allocTable(("Trail_SRV_" + path).c_str());
    if (!srv.isValid()){
        LOG("TrailPass: SRV alloc failed for '%s', using fallback", path.c_str());
        m_textureCache.emplace(path, CachedTexture{ nullptr, m_fallbackSRV });
        return m_fallbackSRV.getGPUHandle(0);
    }
    srv.createTexture2DSRV(tex.Get(), 0);

    auto handle = srv.getGPUHandle(0);
    m_textureCache.emplace(path, CachedTexture{ std::move(tex), std::move(srv) });
    return handle;
}

void TrailPass::render(ID3D12GraphicsCommandList* cmd,
                       const std::vector<TrailInstance>& trails,
                       const Matrix& viewProj,
                       uint32_t width, uint32_t height){
    if (trails.empty() || width == 0 || height == 0) return;

    BEGIN_EVENT(cmd, L"Trail Pass");

    cmd->SetGraphicsRootSignature(m_pipeline.getRootSig());

    ID3D12DescriptorHeap* heaps[] = {
        app->getShaderDescriptors()->getHeap(),
        app->getSamplerHeap()->getHeap()
    };
    cmd->SetDescriptorHeaps(2, heaps);

    cmd->SetGraphicsRootDescriptorTable(TrailPipeline::SLOT_SAMPLER,
        app->getSamplerHeap()->getGPUHandle(ModuleSamplerHeap::LINEAR_WRAP));

    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetIndexBuffer(nullptr);

    const UINT cbStride = cbAlign(sizeof(TrailInstanceCB));
    // Remaining CB slots this frame (other viewports may have already consumed some)
    const UINT remainingSlots = (m_frameCBCursor < MAX_TRAILS) ? MAX_TRAILS - m_frameCBCursor : 0u;
    const UINT maxDraws = std::min((UINT)trails.size(), remainingSlots);

    bool additiveBound = false;
    cmd->SetPipelineState(m_pipeline.getPSO());

    UINT vertexCursor = m_frameVertexCursor;
    UINT drawnCount = 0;
    for (UINT i = 0; i < maxDraws; ++i){
        const TrailInstance& tr = trails[i];
        const UINT vCount = (UINT)tr.vertices.size();
        if (vCount == 0) continue;
        if (vertexCursor + vCount > MAX_TRAIL_VERTICES) break;

        if (tr.additive != additiveBound){
            additiveBound = tr.additive;
            cmd->SetPipelineState(additiveBound ? m_pipeline.getAdditivePSO() : m_pipeline.getPSO());
        }

        uint8_t* dstV = reinterpret_cast<uint8_t*>(m_vbMapped) + (size_t)vertexCursor * m_vbStride;
        memcpy(dstV, tr.vertices.data(), (size_t)vCount * m_vbStride);

        D3D12_VERTEX_BUFFER_VIEW vbv = {};
        vbv.BufferLocation = m_vbRing->GetGPUVirtualAddress() + (UINT64)vertexCursor * m_vbStride;
        vbv.SizeInBytes = vCount * m_vbStride;
        vbv.StrideInBytes = m_vbStride;
        cmd->IASetVertexBuffers(0, 1, &vbv);

        const UINT cbSlot = m_frameCBCursor + drawnCount;
        TrailInstanceCB cb{};
        cb.viewProj = viewProj.Transpose();
        cb.tint = tr.tint;
        void* dstCb = reinterpret_cast<uint8_t*>(m_cbMapped) + cbSlot * cbStride;
        memcpy(dstCb, &cb, sizeof(TrailInstanceCB));

        D3D12_GPU_VIRTUAL_ADDRESS cbVA = m_cbRing->GetGPUVirtualAddress() + cbSlot * cbStride;
        cmd->SetGraphicsRootConstantBufferView(TrailPipeline::SLOT_CB, cbVA);
        cmd->SetGraphicsRootDescriptorTable(TrailPipeline::SLOT_TEXTURE, getOrLoadTexture(tr.texturePath));

        cmd->DrawInstanced(vCount, 1, 0, 0);

        vertexCursor += vCount;
        ++drawnCount;
    }

    // Advance frame-persistent cursors so the next viewport's render() doesn't overwrite our data.
    m_frameVertexCursor = vertexCursor;
    m_frameCBCursor += drawnCount;

    END_EVENT(cmd);
}

#include "Globals.h"
#include "ModuleSamplerHeap.h"
#include "ReadData.h"
#include <d3dx12.h>

bool TrailPipeline::init(ID3D12Device* device){
    return createRootSignature(device) && createPSO(device);
}

bool TrailPipeline::createRootSignature(ID3D12Device* device){
    CD3DX12_DESCRIPTOR_RANGE texRange; texRange .Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE samplerRange; samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
                                                             ModuleSamplerHeap::COUNT, 0);

    CD3DX12_ROOT_PARAMETER params[3];
    params[SLOT_CB ].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
    params[SLOT_TEXTURE].InitAsDescriptorTable(1, &texRange, D3D12_SHADER_VISIBILITY_PIXEL);
    params[SLOT_SAMPLER].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(_countof(params), params, 0, nullptr,
              D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    if (FAILED(hr)){
        if (error) OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
        LOG("TrailPipeline: serialize root sig failed 0x%08X", hr);
        return false;
    }
    hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                      IID_PPV_ARGS(&m_rootSig));
    if (FAILED(hr)){ LOG("TrailPipeline: CreateRootSignature failed 0x%08X", hr); return false; }
    return true;
}

bool TrailPipeline::createPSO(ID3D12Device* device){
    auto vs = DX::ReadData(L"TrailVS.cso");
    auto ps = DX::ReadData(L"TrailPS.cso");

    static const D3D12_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = m_rootSig.Get();
    desc.VS = { vs.data(), vs.size() };
    desc.PS = { ps.data(), ps.size() };
    desc.InputLayout = { layout, _countof(layout) };
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    desc.SampleDesc = { 1, 0 };
    desc.SampleMask = UINT_MAX;

    desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    auto& rt = desc.BlendState.RenderTarget[0];
    rt.BlendEnable = TRUE;
    rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    rt.BlendOp = D3D12_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D12_BLEND_ONE;
    rt.DestBlendAlpha = D3D12_BLEND_ZERO;
    rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    desc.DepthStencilState.DepthEnable = TRUE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    HRESULT hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_pso));
    if (FAILED(hr)){ LOG("TrailPipeline: CreateGraphicsPipelineState failed 0x%08X", hr); return false; }

    auto& art = desc.BlendState.RenderTarget[0];
    art.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    art.DestBlend = D3D12_BLEND_ONE;
    hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_additivePso));
    if (FAILED(hr)){ LOG("TrailPipeline: CreateGraphicsPipelineState (additive) failed 0x%08X", hr); return false; }

    return true;
}
